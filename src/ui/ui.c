#include "ui.h"
#include "ui/popup.h"
#include "ui/notification.h"
#include "data/colors.h"
#include "data/input.h"
#include "data/fonts.h"
#include "util/logger.h"
#include <easylogger.h>
#include <string.h>
#include <math.h>

#define UI_AA_SCALE 2
#define UI_SCISSOR_STACK_SIZE 8

IMPL_ARRLIST(Panel);
IMPL_ARRLIST(UIConfig);

typedef struct {
    PersistantUIData* data;
    char** items;
    size_t count;
    BOOL active;
    float width;
    void* funcdata;
    DropdownSelectFunction func;
    Vector2 origin;
} DropdownMenuData;

typedef enum {
    TIM_ALPHANUMERIC,
    TIM_FLOAT,
    TIM_INT,
    TIM_UINT,
} TextInputMode;

typedef struct {
    char* buffer;
    size_t size;
    BOOL active;
    PersistantUIData* data;
    size_t cursor;
    Vector2 origin;
    float width;
    BOOL submitted;
    TextInputMode mode;
} TextInputData;

static UI* g_primary_ui = NULL;
static UI* g_divider_instance = NULL;
static BOOL g_divider_active = FALSE;
static Vector2 g_ui_cursor = { 0 };
static Vector2 g_ui_position = { 0 };
static Vector2 g_ui_size = { 0 };
static char g_ui_text_buffer[MAX_LINE_WIDTH] = { 0 };
static Popup* g_popup = NULL;
static Popup* g_popup_origin = NULL;
static PersistantUIData* g_active_ui_element = NULL;
static DropdownMenuData g_dropdownmenu_data = { 0 };
static TextInputData g_textinput_data = { 0 };
static BOOL g_was_ui_element_just_used = FALSE;
static BOOL g_ui_disabled = FALSE;
static UI* g_fullscreen_ui = NULL;
static UI* g_current_prendered_ui = NULL;
static ARRLIST_UIConfig g_ui_config = { 0 };
static BOOL g_reset_ui = FALSE;
static char g_dragtext_buf[32] = { 0 };
static PersistantUIData* g_dragtext_owner = NULL;
static PersistantUIData* g_last_click_owner = NULL;
static double g_last_click_time = 0.0;
static RenderTexture2D g_ui_target = { 0 };
static BOOL g_ui_use_target = FALSE;
static BOOL g_ui_drawing = FALSE;
static BOOL g_ui_paused = FALSE;
static float g_ui_scale = 1.0f;
static Vector2 g_ui_origin = { 0 };
static Rectangle g_ui_scissors[UI_SCISSOR_STACK_SIZE] = { 0 };
static int g_ui_scissor_depth = 0;
static BOOL g_ui_panels_rendered = FALSE;
static UI* g_ui_frame_fullscreen = NULL;
static Rectangle g_ui_root = { 0 };
static BOOL g_ui_root_known = FALSE;
static BOOL g_ui_root_tracks_screen = TRUE;

static void ApplyUICamera() {
    Camera2D camera = { 0 };
    camera.zoom = g_ui_scale;
    camera.offset = (Vector2){ g_ui_origin.x * g_ui_scale, g_ui_origin.y * g_ui_scale };
    BeginMode2D(camera);
}

static void ApplyUIScissor() {
    if (g_ui_scissor_depth == 0) return;
    Rectangle r = g_ui_scissors[g_ui_scissor_depth - 1];
    int x0 = (int)floorf(r.x * g_ui_scale);
    int y0 = (int)floorf(r.y * g_ui_scale);
    int x1 = (int)ceilf((r.x + r.width) * g_ui_scale);
    int y1 = (int)ceilf((r.y + r.height) * g_ui_scale);
    BeginScissorMode(x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0);
}

static void BindUIState() {
    if (g_ui_drawing) return;
    if (g_ui_use_target) BeginTextureMode(g_ui_target);
    ApplyUICamera();
    ApplyUIScissor();
    g_ui_drawing = TRUE;
}

static void UnbindUIState() {
    if (!g_ui_drawing) return;
    if (g_ui_scissor_depth > 0) EndScissorMode();
    EndMode2D();
    if (g_ui_use_target) EndTextureMode();
    g_ui_drawing = FALSE;
}

static void SetUIOrigin(float x, float y) {
    g_ui_origin = (Vector2){ x, y };
    if (!g_ui_drawing) return;
    EndMode2D();
    ApplyUICamera();
}

static void PushUIScissor(Rectangle rect) {
    EZ_ASSERT(g_ui_scissor_depth < UI_SCISSOR_STACK_SIZE, "UI scissor stack overflow");
    if (rect.width < 0) rect.width = 0;
    if (rect.height < 0) rect.height = 0;
    if (g_ui_scissor_depth > 0) {
        Rectangle parent = g_ui_scissors[g_ui_scissor_depth - 1];
        float x0 = rect.x > parent.x ? rect.x : parent.x;
        float y0 = rect.y > parent.y ? rect.y : parent.y;
        float x1 = rect.x + rect.width < parent.x + parent.width ? rect.x + rect.width : parent.x + parent.width;
        float y1 = rect.y + rect.height < parent.y + parent.height ? rect.y + rect.height : parent.y + parent.height;
        rect = (Rectangle){ x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0 };
    }
    g_ui_scissors[g_ui_scissor_depth] = rect;
    g_ui_scissor_depth++;
    if (g_ui_drawing) ApplyUIScissor();
}

static void PopUIScissor() {
    EZ_ASSERT(g_ui_scissor_depth > 0, "UI scissor stack underflow");
    g_ui_scissor_depth--;
    if (!g_ui_drawing) return;
    if (g_ui_scissor_depth > 0) ApplyUIScissor();
    else EndScissorMode();
}

static BOOL EnsureUITarget() {
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    if (width <= 0 || height <= 0) return FALSE;
    int target_width = width * UI_AA_SCALE;
    int target_height = height * UI_AA_SCALE;
    if (!IsRenderTextureValid(g_ui_target) ||
        g_ui_target.texture.width != target_width ||
        g_ui_target.texture.height != target_height) {
        if (IsRenderTextureValid(g_ui_target))
            UnloadRenderTexture(g_ui_target);
        g_ui_target = LoadRenderTexture(target_width, target_height);
        if (IsRenderTextureValid(g_ui_target))
            SetTextureFilter(g_ui_target.texture, TEXTURE_FILTER_BILINEAR);
        g_ui_panels_rendered = FALSE;
    }
    return IsRenderTextureValid(g_ui_target);
}

static Rectangle UIRootRect() {
    if (!g_ui_root_known || g_ui_root_tracks_screen)
        return (Rectangle){ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
    return g_ui_root;
}

UI* GetLeftUI(UI* ui) {
    return (UI*)(ui->left);
}

UI* GetRightUI(UI* ui) {
    return (UI*)(ui->right);
}

void SetPrimaryUI(UI* ui) {
    g_primary_ui = ui;
}

UI* GetPrimaryUI() {
    return g_primary_ui;
}

void ResizeUI() {
    EnsureUITarget();
}

UI* GenerateUI() {
    UI* ui = EZ_ALLOC(1, sizeof(UI));
    return ui;
}

void SetupPanel(Panel* panel, const char* name) {
    strcpy(panel->name, name);
}

BOOL UIRequestsBlockInput() {
    return g_popup != NULL || g_dropdownmenu_data.active || g_textinput_data.active;
}

void UpdateUI(UI* ui) {
    EZ_ASSERT((ui->left && ui->right) || (!ui->left && !ui->right), "UI branches must be split evenly");
    if (ui->left && ui->right) {
        // update further down
        UpdateUI((UI*)(ui->left));
        UpdateUI((UI*)(ui->right));

        // handle hovering and active dragging
        if (UIRequestsBlockInput()) BlockInputs();
        size_t buffer = 5;
        if (!g_divider_active) {
            if (ui->vertical) {
                if ((size_t)GetMouseX() < ui->x + ui->w &&
                    (size_t)GetMouseX() > ui->x &&
                    (size_t)GetMouseY() < ui->y + ui->divide + buffer &&
                    (size_t)GetMouseY() > ui->y + ui->divide - buffer) {
                    g_divider_instance = ui;
                    if (InputButtonPressed(IK_MOUSELEFT)) g_divider_active = TRUE;
                }
            } else {
                if ((size_t)GetMouseY() < ui->y + ui->h &&
                    (size_t)GetMouseY() > ui->y &&
                    (size_t)GetMouseX() < ui->x + ui->divide + buffer &&
                    (size_t)GetMouseX() > ui->x + ui->divide - buffer) {
                    g_divider_instance = ui;
                    if (InputButtonPressed(IK_MOUSELEFT)) g_divider_active = TRUE;
                }
            }
        } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            g_divider_active = FALSE;
        }
    }

    // divisor dragging
    if (g_divider_instance == ui && g_divider_active) {
        if (ui->vertical) {
            if ((size_t)GetMouseY() < ui->y + ui->h && (size_t)GetMouseY() > ui->y)
                ui->divide = GetMouseY() - ui->y;
        } else {
            if ((size_t)GetMouseX() < ui->x + ui->w && (size_t)GetMouseX() > ui->x)
                ui->divide = GetMouseX() - ui->x;
        }
    }

    // update panel
    if (UIRequestsBlockInput()) BlockInputs();
	for (size_t i = 0; i < ui->panels.size; i++)
	    if (ui->panels.data[i].update) ui->panels.data[i].update(ui->w, ui->h);
    UnblockInputs();
}

static void LayoutUI(UI* ui, size_t x, size_t y, size_t w, size_t h) {
    EZ_ASSERT((ui->left && ui->right) || (!ui->left && !ui->right), "UI branches must be split evenly");
    ui->w = w;
    ui->h = h;
    ui->x = x;
    ui->y = y;
    if (ui->left && ui->right) {
        if (ui->vertical) {
            LayoutUI((UI*)(ui->left), x, y, w, ui->divide);
            LayoutUI((UI*)(ui->right), x, y + ui->divide, w, h - ui->divide);
        } else {
            LayoutUI((UI*)(ui->left), x, y, ui->divide, h);
            LayoutUI((UI*)(ui->right), x + ui->divide, y, w - ui->divide, h);
        }
    }
}

static void DrawUIChrome(UI* ui) {
    size_t x = ui->x;
    size_t y = ui->y;
    size_t w = ui->w;
    size_t h = ui->h;

    if (ui->left && ui->right) {
        // draw inner ui
        DrawUIChrome((UI*)(ui->left));
        DrawUIChrome((UI*)(ui->right));

        // draw divider
        if (ui == g_divider_instance) {
            size_t sth = 2;
            if (ui->vertical) {
                DrawLineEx(
                    (Vector2){x, y + ui->divide + (sth/2)},
                    (Vector2){x + w, y + ui->divide + (sth/2)},
                    sth,
                    g_divider_active ? MappedColor(PANEL_DIVIDER_ACTIVE_COLOR) : MappedColor(PANEL_DIVIDER_HOVER_COLOR));
            } else {
                DrawLineEx(
                    (Vector2){x + ui->divide + (sth/2), y},
                    (Vector2){x + ui->divide + (sth/2), y + h},
                    sth,
                    g_divider_active ? MappedColor(PANEL_DIVIDER_ACTIVE_COLOR) : MappedColor(PANEL_DIVIDER_HOVER_COLOR));
            }
            if (!g_divider_active) g_divider_instance = NULL;
        }
    } else {
        float namebar_dif = ui->panels.data[ui->selected].name[0] != 0 && !ui->panels.data[ui->selected].flush ? NAMEBAR_HEIGHT : 0.0f;
        if (namebar_dif > 0.0) {
            DrawRectangle(x, y, w, NAMEBAR_HEIGHT, MappedColor(PANEL_NB_COLOR));
			int xplus = x;
			for (size_t i = 0; i < ui->panels.size; i++) {
				if (ui->panels.data[i].name[0] == 0) continue;
				BOOL selected = ui->selected == i;
				Color deselectedtc = MappedColor(UI_TEXT_COLOR);
				deselectedtc.a = 150;
				float tag_len = MeasureTextEx(FontAsset(), ui->panels.data[i].name, LINE_HEIGHT, 0).x;
				if (InputButtonReleased(IK_MOUSELEFT))
					if (GetMouseX() > xplus && GetMouseX() < xplus + tag_len + 20 && GetMouseY() > (int)y && GetMouseY() < (int)y + NAMEBAR_HEIGHT)
						ui->selected = i;
				DrawRectangle(xplus, y + NAMEBAR_HEIGHT / 2, tag_len + 20, NAMEBAR_HEIGHT / 2, selected ? MappedColor(PANEL_NBGS_COLOR) : MappedColor(PANEL_NBG_COLOR));
				DrawRectangleRounded((Rectangle){xplus, y, tag_len + 20, 3 * NAMEBAR_HEIGHT / 4}, 10, 10, selected ? MappedColor(PANEL_NBGS_COLOR) : MappedColor(PANEL_NBG_COLOR));
				DrawTextEx(FontAsset(), ui->panels.data[i].name, (Vector2){ xplus + 10, y + NAMEBAR_HEIGHT - LINE_HEIGHT - 2 }, LINE_HEIGHT, 0, selected ? MappedColor(UI_TEXT_COLOR) : deselectedtc);	
				xplus += tag_len + 20 + 5;
			}
        }
        size_t th = 1;
        if (y != 0) DrawLineEx((Vector2){x, y + (th/2)}, (Vector2){x + w, y + (th/2)}, th, MappedColor(PANEL_DIVIDER_COLOR));
        if (x != 0) DrawLineEx((Vector2){x + (th/2), y}, (Vector2){x + (th/2), y + h}, th, MappedColor(PANEL_DIVIDER_COLOR));
        if (x + w < (size_t)GetScreenWidth()) DrawLineEx((Vector2){x + w - (th/2), y + h}, (Vector2){x + w - (th/2), y}, th, MappedColor(PANEL_DIVIDER_COLOR));
        if (y + h < (size_t)GetScreenHeight()) DrawLineEx((Vector2){x + w, y + h - (th/2)}, (Vector2){x, y + h - (th/2)}, th, MappedColor(PANEL_DIVIDER_COLOR));
    }
}

static void DrawPopup(size_t x, size_t y, size_t w, size_t h) {
    EZ_ASSERT(g_popup != NULL, "Cannot draw a null popup!");
    DrawRectangle(x, y, w, h, (Color){ 255, 255, 255, 100 });
    if (g_popup->behavior != NULL) {
        int result = g_popup->behavior(x, y, w, h);
        if (result >= (int)g_popup->options) {
            CleanPopup(g_popup_origin);
            g_popup_origin = NULL;
            g_popup = NULL;
        } else if (result >= 0) {
            g_popup = ((Popup**)g_popup->results)[result];
        }
    }
}

static void DrawDropdownMenu() {
    Vector2 cursor = g_dropdownmenu_data.origin;
    cursor.y += LINE_HEIGHT - 2;
    for (size_t i = 0; i < g_dropdownmenu_data.count; i++) {
        if (i == g_dropdownmenu_data.data->arbitrary_counter) continue;
        Vector2 text_size = MeasureTextEx(FontAsset(), g_dropdownmenu_data.items[i], LINE_HEIGHT, 0);
        float w = g_dropdownmenu_data.width < text_size.x + 10 ? text_size.x + 10 : g_dropdownmenu_data.width;
        Color color = MappedColor(UI_DROPDOWN_MENU_COLOR);
        if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){cursor.x, cursor.y, w, LINE_HEIGHT})) {
            if (InputButtonDown(IK_MOUSELEFT)) color = MappedColor(UI_DROPDOWN_MENU_PRS_COLOR);
            else color = MappedColor(UI_DROPDOWN_MENU_HVR_COLOR);
            if (InputButtonReleased(IK_MOUSELEFT)) g_dropdownmenu_data.data->arbitrary_counter = g_dropdownmenu_data.func(g_dropdownmenu_data.funcdata, i, FALSE);
        }
        DrawRectangle(cursor.x, cursor.y, w, LINE_HEIGHT, color);
        DrawRectangle(cursor.x + 2, cursor.y + 2, 2, LINE_HEIGHT - 4, MappedColor(UI_DIVIDER_COLOR));
        DrawTextEx(FontAsset(), g_dropdownmenu_data.items[i], (Vector2){cursor.x + 8, cursor.y + 0}, LINE_HEIGHT, 0, MappedColor(UI_TEXT_COLOR));
        cursor.y += LINE_HEIGHT;
    }
    if (InputButtonReleased(IK_MOUSELEFT) && g_dropdownmenu_data.active == 2) {
        g_dropdownmenu_data.func(g_dropdownmenu_data.funcdata, (size_t)-1, TRUE);
        g_dropdownmenu_data.active = FALSE;
    } else if (InputButtonReleased(IK_MOUSELEFT)) g_dropdownmenu_data.active = 2;
}

static inline BOOL IsTextable(int c) {
    return c >= 32 && c <= 126;
}

static void HandleTextInput() {
    if (g_textinput_data.cursor > strlen(g_textinput_data.buffer)) g_textinput_data.cursor = 0;
    int c;
    size_t pre_cursor = g_textinput_data.cursor;
    static float backspace_timer = 0.0f;
    backspace_timer += GetFrameTime();
    if (!InputKeyDown(IK_BACKSPACE)) backspace_timer = 0.0f;
    while ((c = GetCharPressed()) != 0) {
        BOOL allowed = FALSE;
        switch (g_textinput_data.mode) {
            case TIM_FLOAT: allowed = (c >= '0' && c <= '9') || c == '.' || c == '-'; break;
            case TIM_INT: allowed = (c >= '0' && c <= '9') || c == '-'; break;
            case TIM_UINT: allowed = (c >= '0' && c <= '9'); break;
            case TIM_ALPHANUMERIC:
            default: allowed = IsTextable(c); break;
        }
        if (!allowed) continue;
        if (g_textinput_data.cursor >= g_textinput_data.size - 1) continue;
        if (g_textinput_data.cursor < strlen(g_textinput_data.buffer)) {
            for (size_t i = strlen(g_textinput_data.buffer); i > g_textinput_data.cursor; i--) {
                g_textinput_data.buffer[i] = g_textinput_data.buffer[i - 1];
            }
        }
        char prev = g_textinput_data.buffer[g_textinput_data.cursor];
        g_textinput_data.buffer[g_textinput_data.cursor] = (char)c;
        g_textinput_data.cursor++;
        g_textinput_data.buffer[g_textinput_data.cursor] = prev;
    }
    if (InputKeyPressed(IK_LEFT) && g_textinput_data.cursor > 0) g_textinput_data.cursor--;
    if (InputKeyPressed(IK_RIGHT) && g_textinput_data.cursor < strlen(g_textinput_data.buffer)) g_textinput_data.cursor++;
    if (InputKeyPressed(IK_UP)) g_textinput_data.cursor = 0;
    if (InputKeyPressed(IK_DOWN)) g_textinput_data.cursor = strlen(g_textinput_data.buffer);
    if ((backspace_timer > 0.5f || InputKeyPressed(IK_BACKSPACE)) && g_textinput_data.cursor > 0) {
        g_textinput_data.buffer[g_textinput_data.cursor - 1] = '\0';
        for (size_t i = g_textinput_data.cursor - 1; i < g_textinput_data.size - 1; i++)
            g_textinput_data.buffer[i] = g_textinput_data.buffer[i+1];
        g_textinput_data.cursor--;
    }
    if (InputKeyPressed(IK_ENTER) || (InputButtonPressed(IK_MOUSELEFT) && !CheckCollisionPointRec(
        GetMousePosition(),
        (Rectangle){ g_textinput_data.origin.x, g_textinput_data.origin.y, g_textinput_data.width, LINE_HEIGHT - 2 }))) {
        g_textinput_data.active = FALSE;
        if (InputKeyPressed(IK_ENTER)) {
            g_textinput_data.submitted = TRUE;
        } else {
            g_textinput_data.data = NULL;
        }
    }
    if (InputButtonPressed(IK_MOUSELEFT) && CheckCollisionPointRec(
        GetMousePosition(),
        (Rectangle){ g_textinput_data.origin.x, g_textinput_data.origin.y, g_textinput_data.width, LINE_HEIGHT - 2 })) {
        g_textinput_data.cursor = strlen(g_textinput_data.buffer);
        float text_start_pos = 2.0f;
        Vector2 text_size = MeasureTextEx(FontAsset(), g_textinput_data.buffer, LINE_HEIGHT, 0);
        if (text_size.x > g_textinput_data.width) text_start_pos -= (text_size.x - g_textinput_data.width) + 4.0f;
        for (size_t i = 0; i < g_textinput_data.size; i++) {
            if (g_textinput_data.buffer[i] == '\0') break;
            char b[1024] = { 0 };
            memcpy(b, g_textinput_data.buffer, i + 1);
            Vector2 tsize = MeasureTextEx(FontAsset(), b, LINE_HEIGHT, 0);
            if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){ g_textinput_data.origin.x + text_start_pos, g_textinput_data.origin.y, tsize.x, LINE_HEIGHT })) {
                g_textinput_data.cursor = i;
                break;
            }
        }
    }
    if (pre_cursor != g_textinput_data.cursor) g_textinput_data.data->arbitrary_timer = 0.0f;
}

static void RenderUIPanels_helper(UI* ui) {
    if (ui->left && ui->right) {
        RenderUIPanels_helper((UI*)(ui->left));
        RenderUIPanels_helper((UI*)(ui->right));
        return;
    }
    Panel* panel = &(ui->panels.data[ui->selected]);
    SetUIOrigin(0, 0);
    DrawRectangle(ui->x, ui->y, ui->w, ui->h, MappedColor(PANEL_BG_COLOR));
    if (!panel->draw) return;
    g_ui_cursor = (Vector2){ 10, 5 };
    g_ui_position = (Vector2){ ui->x, ui->y };
    g_ui_size = (Vector2){ ui->w, ui->h };
    g_current_prendered_ui = ui;
    if (panel->name[0] != 0 && !panel->flush) {
        g_ui_position.y += NAMEBAR_HEIGHT;
        g_ui_size.y -= NAMEBAR_HEIGHT;
    }
    PushUIScissor((Rectangle){ g_ui_position.x, g_ui_position.y, g_ui_size.x, g_ui_size.y });
    SetUIOrigin(g_ui_position.x, g_ui_position.y);
    float uih = ui->h;
    if (panel->scrollable && panel->autoheight != 0.0f) {
        if (HoveredPanel() && strcmp(HoveredPanel(), panel->name) == 0) {
            if (InputKeyPressed(IK_ENTER)) panel->autoscroll = 0.0f;
            panel->autoscroll -= 18.0f * GetMouseWheelMove();
        }
        if (panel->autoscroll > panel->autoheight - uih + 50)
            panel->autoscroll = panel->autoheight - uih + 50;
        if (panel->autoscroll < 0)
            panel->autoscroll = 0.0f;
        UIMoveCursor(0, 0 - panel->autoscroll);
    }
    panel->autoheight = g_ui_cursor.y;
    panel->draw(ui->w, uih);
    panel->autoheight = g_ui_cursor.y - panel->autoheight;
    PopUIScissor();
    SetUIOrigin(0, 0);
}

static void RenderUIPanels(UI* ui) {
    if (UIRequestsBlockInput()) BlockInputs();
    g_ui_frame_fullscreen = g_fullscreen_ui;
    UI* root = g_ui_frame_fullscreen ? g_ui_frame_fullscreen : ui;
    Rectangle rect = UIRootRect();
    LayoutUI(root, rect.x, rect.y, rect.width, rect.height);
    g_ui_scissor_depth = 0;
    g_ui_origin = (Vector2){ 0, 0 };
    g_ui_paused = FALSE;
    BindUIState();
    if (g_ui_use_target) ClearBackground((Color){ 0, 0, 0, 0 });
    RenderUIPanels_helper(root);
    g_ui_scissor_depth = 0;
    UnbindUIState();
    g_ui_panels_rendered = TRUE;
    UnblockInputs();
}

void PreRenderUI(UI* ui) {
    g_ui_use_target = EnsureUITarget();
    g_ui_scale = g_ui_use_target ? (float)UI_AA_SCALE : 1.0f;
    if (!g_ui_use_target) return;
    RenderUIPanels(ui);
}

void DrawUI(UI* ui, size_t x, size_t y, size_t w, size_t h) {
    g_ui_use_target = EnsureUITarget();
    g_ui_scale = g_ui_use_target ? (float)UI_AA_SCALE : 1.0f;
    g_ui_root = (Rectangle){ x, y, w, h };
    g_ui_root_known = TRUE;
    g_ui_root_tracks_screen = x == 0 && y == 0 && w == (size_t)GetScreenWidth() && h == (size_t)GetScreenHeight();

    if (!g_ui_panels_rendered) RenderUIPanels(ui);
    g_ui_panels_rendered = FALSE;

    if (InputButtonUp(IK_MOUSELEFT)) g_active_ui_element = NULL;
    if (UIRequestsBlockInput()) BlockInputs();
    UI* root = g_ui_frame_fullscreen ? g_ui_frame_fullscreen : ui;
    LayoutUI(root, x, y, w, h);
    g_ui_scissor_depth = 0;
    g_ui_origin = (Vector2){ 0, 0 };
    g_ui_paused = FALSE;
    BindUIState();

    DrawUIChrome(root);
    if (g_popup != NULL) {
        UnblockInputs();
        if (g_dropdownmenu_data.active) BlockInputs();
        DrawPopup(x, y, w, h);
    }
    if (g_dropdownmenu_data.active) {
        UnblockInputs();
        DrawDropdownMenu();
    }
    if (g_textinput_data.active) {
        UnblockInputs();
        HandleTextInput();
    }
    DrawNotifications();
    g_ui_scissor_depth = 0;
    UnbindUIState();
    if (g_ui_use_target) {
        DrawTexturePro(
            g_ui_target.texture,
            (Rectangle){ 0, (float)g_ui_target.texture.height,
                         (float)g_ui_target.texture.width,
                         -(float)g_ui_target.texture.height },
            (Rectangle){ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() },
            (Vector2){ 0, 0 },
            0.0f,
            WHITE);
    }
}

void DestroyUI(UI* ui) {
    if (IsRenderTextureValid(g_ui_target)) {
        UnloadRenderTexture(g_ui_target);
        g_ui_target = (RenderTexture2D){ 0 };
    }
    g_ui_panels_rendered = FALSE;
    if (g_ui_frame_fullscreen == ui) g_ui_frame_fullscreen = NULL;
    if (g_popup_origin != NULL) {
        CleanPopup(g_popup_origin);
        g_popup_origin = NULL;
    }
    if (ui->left) DestroyUI((UI*)ui->left);
    if (ui->right) DestroyUI((UI*)ui->right);
	ARRLIST_Panel_clear(&(ui->panels));
	ARRLIST_UIConfig_clear(&g_ui_config);
    EZ_FREE(ui);
}

void WipeUI(UI* ui) {
    if (!ui) return;
    if (ui->left) WipeUI((UI*)ui->left);
    if (ui->right) WipeUI((UI*)ui->right);
    if (g_ui_frame_fullscreen == ui) g_ui_frame_fullscreen = NULL;
	ARRLIST_Panel_clear(&(ui->panels));
    EZ_FREE(ui);
}

void DestroyPanel(Panel* panel) {
    if (panel->clean) panel->clean();
}

static const char* HoveredPanelHelper(UI* ui) {
    if (!ui) return NULL;
    if (!ui->left && !ui->right) {
        if (GetMousePosition().x > ui->x &&
            GetMousePosition().x < ui->x + ui->w &&
            GetMousePosition().y > ui->y &&
            GetMousePosition().y < ui->y + ui->h) {
            return ui->panels.data[ui->selected].name;
        }
    } else {
        const char* l = HoveredPanelHelper((UI*)ui->left);
        if (l) return l;
        return HoveredPanelHelper((UI*)ui->right);
    }
    return NULL;
}

const char* HoveredPanel() {
    if (g_fullscreen_ui) return g_fullscreen_ui->panels.data[g_fullscreen_ui->selected].name;
    return HoveredPanelHelper(g_primary_ui);
}

void ClearJustUsedUI() {
    g_was_ui_element_just_used = FALSE;
}

BOOL UIWasJustUsed() {
    return g_was_ui_element_just_used;
}

void UIDrawText(const char* text, ...) {
    va_list args;
    va_start(args, text);
    vsnprintf(g_ui_text_buffer, MAX_LINE_WIDTH - 1, text, args);
    DrawTextEx(
        FontAsset(), g_ui_text_buffer, g_ui_cursor, LINE_HEIGHT, 0, 
        g_ui_disabled ? MappedColor(UI_TEXT_DISABLED) : MappedColor(UI_TEXT_COLOR));
    g_ui_cursor.y += LINE_HEIGHT;
    g_ui_cursor.x = 10;
}

void UIDrawTextWrapped(float width, const char* text, ...) {
    va_list args;
    va_start(args, text);
    vsnprintf(g_ui_text_buffer, MAX_LINE_WIDTH - 1, text, args);
    char* phrase = g_ui_text_buffer;
    char buffer[MAX_LINE_WIDTH] = { 0 };
    int i = 0;
    char* oldp = phrase;
    size_t oldi = i;
    BOOL first = TRUE;
    while (phrase[0] != '\0') {
        buffer[i] = phrase[0];
        if (buffer[i] == ' ' || buffer[i] == '\n') {
            float twidth = MeasureTextEx(FontAsset(), buffer, LINE_HEIGHT, 0).x;
            if (twidth > width || buffer[i] == '\n') {
                buffer[i] = ' ';
                if (first || twidth <= width) {
                    oldi = i;
                    oldp = phrase;
                }
                buffer[oldi] = '\0';
                DrawTextEx(FontAsset(), buffer, g_ui_cursor, LINE_HEIGHT, 0, MappedColor(UI_TEXT_COLOR));
                g_ui_cursor.y += LINE_HEIGHT;
                memset(buffer, '\0', MAX_LINE_WIDTH);
                i = -1;
                phrase = oldp;
                first = TRUE;
            } else {
                oldp = phrase;
                oldi = i;
                first = FALSE;
            }
        }
        phrase++;
        i++;
    }
    DrawTextEx(FontAsset(), buffer, g_ui_cursor, LINE_HEIGHT, 0, MappedColor(UI_TEXT_COLOR));
    g_ui_cursor.y += LINE_HEIGHT;
    g_ui_cursor.x = 10;
}

void UIDrawSubtleText(const char* text, ...) {
    va_list args;
    va_start(args, text);
    vsnprintf(g_ui_text_buffer, MAX_LINE_WIDTH - 1, text, args);
    DrawTextEx(FontAsset(), g_ui_text_buffer, g_ui_cursor, LINE_HEIGHT, 0, MappedColor(UI_SUBTLE_TEXT_COLOR));
    g_ui_cursor.y += LINE_HEIGHT;
    g_ui_cursor.x = 10;
}

void UIDrawItalicText(const char* text, ...) {
    va_list args;
    va_start(args, text);
    vsnprintf(g_ui_text_buffer, MAX_LINE_WIDTH - 1, text, args);
    DrawTextEx(FontAssetItalic(), g_ui_text_buffer, g_ui_cursor, LINE_HEIGHT, 0, MappedColor(UI_DIVIDER_COLOR));
    g_ui_cursor.y += LINE_HEIGHT;
    g_ui_cursor.x = 10;
}

BOOL UIDragFloat_(PersistantUIData* data, float* value, float min, float max, float speed, size_t w) {
    if (g_textinput_data.submitted && g_textinput_data.data == data) {
        g_textinput_data.active = FALSE;
        g_textinput_data.data = NULL;
    }
    BOOL ret = FALSE;
    Rectangle rect = {g_ui_cursor.x + g_ui_position.x, g_ui_cursor.y + g_ui_position.y + 2, w, LINE_HEIGHT - 4};
    if (g_dragtext_owner == data && g_textinput_data.data != data) {
        float prev = *value;
        *value = (float)atof(g_dragtext_buf);
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        if (prev != *value) ret = TRUE;
        g_dragtext_owner = NULL;
        g_was_ui_element_just_used = TRUE;
    }
    BOOL text_active = (g_dragtext_owner == data);
    if (!text_active && !g_ui_disabled && InputButtonPressed(IK_MOUSELEFT) &&
        CheckCollisionPointRec(GetMousePosition(), rect)) {
        double now = GetTime();
        BOOL is_double = (g_last_click_owner == data) && (now - g_last_click_time < 0.3);
        g_last_click_owner = data;
        g_last_click_time = now;
        if (is_double) {
            snprintf(g_dragtext_buf, sizeof(g_dragtext_buf), "%g", *value);
            g_textinput_data = (TextInputData){
                g_dragtext_buf, sizeof(g_dragtext_buf), TRUE, data, strlen(g_dragtext_buf),
                (Vector2){rect.x, rect.y}, w, FALSE, TIM_FLOAT
            };
            data->arbitrary_timer = 0.0f;
            g_dragtext_owner = data;
        } else {
            g_active_ui_element = data;
        }
    }
    if (!text_active && !g_ui_disabled && InputButtonPressed(IK_MOUSERIGHT) &&
        CheckCollisionPointRec(GetMousePosition(), rect)) {
        float prev = *value;
        *value = 0.0f;
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        if (prev != *value) ret = TRUE;
        g_was_ui_element_just_used = TRUE;
    }
    if (!text_active && g_active_ui_element == data) {
        float prev = *value;
        *value += GetMouseDelta().x * speed;
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        if (prev != *value) ret = TRUE;
        g_was_ui_element_just_used = TRUE;
    }
    DrawRectangle(g_ui_cursor.x, g_ui_cursor.y + 1, w, LINE_HEIGHT - 2,
        g_ui_disabled ? MappedColor(UI_BOX_DISABLED) :
        text_active ? MappedColor(UI_TEXT_INPUT_FOCUS_COLOR) : MappedColor(UI_DRAG_FLOAT_COLOR));
    if (text_active) {
        Vector2 buf_size = MeasureTextEx(FontAsset(), g_dragtext_buf, LINE_HEIGHT, 0);
        float xstart = (w - buf_size.x) / 2.0f;
        if (xstart < 2.0f) xstart = 2.0f;
        DrawTextEx(FontAsset(), g_dragtext_buf, (Vector2){g_ui_cursor.x + xstart, g_ui_cursor.y}, LINE_HEIGHT, 0, MappedColor(UI_TEXT_COLOR));
        const float s_cursor_limit = 0.5f;
        data->arbitrary_timer += GetFrameTime();
        if (data->arbitrary_timer < s_cursor_limit / 2.0f) {
            char b[32] = { 0 };
            memcpy(b, g_dragtext_buf, g_textinput_data.cursor);
            float cx = g_ui_cursor.x + xstart + MeasureTextEx(FontAsset(), b, LINE_HEIGHT, 0).x;
            DrawRectangle(cx, g_ui_cursor.y + 2, 2, LINE_HEIGHT - 5, MappedColor(UI_TEXT_COLOR));
        } else if (data->arbitrary_timer > s_cursor_limit) {
            data->arbitrary_timer = 0.0f;
        }
    } else {
        char buffer[32] = { 0 };
        snprintf(buffer, 32, "%.2f", *value);
        Vector2 text_size = MeasureTextEx(FontAsset(), buffer, LINE_HEIGHT, 0);
        DrawTextEx(FontAsset(), buffer, (Vector2){ g_ui_cursor.x + (w/2) - (text_size.x/2), g_ui_cursor.y }, LINE_HEIGHT, 0,
            g_ui_disabled ? MappedColor(UI_TEXT_DISABLED) : MappedColor(UI_TEXT_COLOR));
    }
    g_ui_cursor.y += LINE_HEIGHT;
    g_ui_cursor.x = 10;
    return ret;
}

BOOL UIDragFloatLabeled_(PersistantUIData* data, const char* label, float* value, float min, float max, float speed, size_t w) {
    UIDrawText(label);
    float xdif = MeasureTextEx(FontAsset(), label, LINE_HEIGHT, 0).x;
    UIMoveCursor(xdif + 5, -LINE_HEIGHT);
    return UIDragFloat_(data, value, min, max, speed, w - 5 - xdif);
}

void UISetCursor(float x, float y) {
    g_ui_cursor.x = x;
    g_ui_cursor.y = y;
}

void UISetPosition(float x, float y) {
    g_ui_position.x = x;
    g_ui_position.y = y;
}

void UIMoveCursor(float x, float y) {
    g_ui_cursor.x += x;
    g_ui_cursor.y += y;
}

Vector2 UIGetCursor() {
    return g_ui_cursor;
}

Vector2 UIGetPosition() {
    return g_ui_position;
}

Vector2 UIGetSize() {
    return g_ui_size;
}

BOOL UICheckbox(BOOL* value) {
    BOOL changed = FALSE;
    if (InputButtonPressed(IK_MOUSELEFT) &&
        CheckCollisionPointRec(
            GetMousePosition(),
            (Rectangle){g_ui_cursor.x + g_ui_position.x + 2, g_ui_cursor.y + g_ui_position.y + 2, LINE_HEIGHT - 4, LINE_HEIGHT - 4})) {
        *value = !(*value);
        g_was_ui_element_just_used = TRUE;
        changed = TRUE;
    }
	DrawRectangle(g_ui_cursor.x + 2, g_ui_cursor.y + 2, LINE_HEIGHT - 4, LINE_HEIGHT - 4, MappedColor(UI_CHECKBOX_COLOR));
	if (*value) {
		float cmx = g_ui_cursor.x + 2.0f;
		float cmy = g_ui_cursor.y + 2.0f;
		DrawRectanglePro((Rectangle){cmx + 4, cmy + 4, 8, 4}, (Vector2){ 0, 0 }, 45.0f, MappedColor(UI_CHECKMARK_COLOR)); 
		DrawRectanglePro((Rectangle){cmx - 9, cmy + 0, 4, 12}, (Vector2){ -16, 16 }, 45.0f, MappedColor(UI_CHECKMARK_COLOR)); 
	}
    g_ui_cursor.y += LINE_HEIGHT;
    g_ui_cursor.x = 10;
    return changed;
}

BOOL UICheckboxLabeled(const char* label, BOOL* value) {
    float prex = g_ui_cursor.x;
    UIDrawText(label);
    UISetCursor(prex, g_ui_cursor.y);
    float xdif = MeasureTextEx(FontAsset(), label, LINE_HEIGHT, 0).x;
    UIMoveCursor(xdif + 5, -LINE_HEIGHT);
	return UICheckbox(value);
}

BOOL UIDragUInt_(PersistantUIData* data, uint32_t* value, uint32_t min, uint32_t max, uint32_t speed, size_t w) {
    if (g_textinput_data.submitted && g_textinput_data.data == data) {
        g_textinput_data.active = FALSE;
        g_textinput_data.data = NULL;
    }
    BOOL ret = FALSE;
    if (g_dragtext_owner == data && g_textinput_data.data != data) {
        uint32_t prev = *value;
        *value = (uint32_t)strtoul(g_dragtext_buf, NULL, 10);
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        if (prev != *value) ret = TRUE;
        g_dragtext_owner = NULL;
        g_was_ui_element_just_used = TRUE;
    }
    BOOL text_active = (g_dragtext_owner == data);
    DrawRectangle(g_ui_cursor.x, g_ui_cursor.y + 1, w, LINE_HEIGHT - 2,
        text_active ? MappedColor(UI_TEXT_INPUT_FOCUS_COLOR) : MappedColor(UI_DRAG_INT_COLOR));
    if (w > 20) {
        w -= 20;
        if (!text_active) {
            DrawTriangle(
                (Vector2){g_ui_cursor.x + 5 + w, g_ui_cursor.y + LINE_HEIGHT/2.0f + 2},
                (Vector2){g_ui_cursor.x + 10 + w, g_ui_cursor.y + LINE_HEIGHT - 2},
                (Vector2){g_ui_cursor.x + 15 + w, g_ui_cursor.y + LINE_HEIGHT/2.0f + 2},
                MappedColor(UI_TEXT_COLOR));
            DrawTriangle(
                (Vector2){g_ui_cursor.x + 15 + w, g_ui_cursor.y + LINE_HEIGHT/2.0f - 2},
                (Vector2){g_ui_cursor.x + 10 + w, g_ui_cursor.y + 2},
                (Vector2){g_ui_cursor.x + 5 + w, g_ui_cursor.y + LINE_HEIGHT/2.0f - 2},
                MappedColor(UI_TEXT_COLOR));
            if (InputButtonPressed(IK_MOUSELEFT) && CheckCollisionPointRec(
                GetMousePosition(),
                (Rectangle){ g_ui_cursor.x + g_ui_position.x + w, g_ui_position.y + g_ui_cursor.y, 20, LINE_HEIGHT/2.0f })) {
                if (*value < max) *value += 1;
                ret = TRUE;
            } else if (InputButtonPressed(IK_MOUSELEFT) && CheckCollisionPointRec(
                GetMousePosition(),
                (Rectangle){ g_ui_cursor.x + g_ui_position.x + w, g_ui_position.y + g_ui_cursor.y + LINE_HEIGHT/2.0f, 20, LINE_HEIGHT/2.0f })) {
                if (*value > min) *value -= 1;
                ret = TRUE;
            }
        }
    }
    Rectangle rect = {g_ui_cursor.x + g_ui_position.x, g_ui_cursor.y + g_ui_position.y + 2, w, LINE_HEIGHT - 4};
    if (!text_active && InputButtonPressed(IK_MOUSELEFT) && CheckCollisionPointRec(GetMousePosition(), rect)) {
        double now = GetTime();
        BOOL is_double = (g_last_click_owner == data) && (now - g_last_click_time < 0.3);
        g_last_click_owner = data;
        g_last_click_time = now;
        if (is_double) {
            snprintf(g_dragtext_buf, sizeof(g_dragtext_buf), "%u", *value);
            g_textinput_data = (TextInputData){
                g_dragtext_buf, sizeof(g_dragtext_buf), TRUE, data, strlen(g_dragtext_buf),
                (Vector2){rect.x, rect.y}, w, FALSE, TIM_UINT
            };
            data->arbitrary_timer = 0.0f;
            g_dragtext_owner = data;
        } else {
            g_active_ui_element = data;
        }
    }
    if (!text_active && !g_ui_disabled && InputButtonPressed(IK_MOUSERIGHT) &&
        CheckCollisionPointRec(GetMousePosition(), rect)) {
        uint32_t prev = *value;
        *value = 0;
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        if (prev != *value) ret = TRUE;
        g_was_ui_element_just_used = TRUE;
    }
    if (!text_active && g_active_ui_element == data) {
        uint32_t prev = *value;
        if (GetMouseDelta().x * speed < 0 && GetMouseDelta().x * speed * -1 > *value)
            *value = 0;
        else
            *value += GetMouseDelta().x * speed;
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        if (prev != *value) ret = TRUE;
        g_was_ui_element_just_used = TRUE;
    }
    if (text_active) {
        Vector2 buf_size = MeasureTextEx(FontAsset(), g_dragtext_buf, LINE_HEIGHT, 0);
        float xstart = (w - buf_size.x) / 2.0f;
        if (xstart < 2.0f) xstart = 2.0f;
        DrawTextEx(FontAsset(), g_dragtext_buf, (Vector2){g_ui_cursor.x + xstart, g_ui_cursor.y}, LINE_HEIGHT, 0, MappedColor(UI_TEXT_COLOR));
        const float s_cursor_limit = 0.5f;
        data->arbitrary_timer += GetFrameTime();
        if (data->arbitrary_timer < s_cursor_limit / 2.0f) {
            char b[32] = { 0 };
            memcpy(b, g_dragtext_buf, g_textinput_data.cursor);
            float cx = g_ui_cursor.x + xstart + MeasureTextEx(FontAsset(), b, LINE_HEIGHT, 0).x;
            DrawRectangle(cx, g_ui_cursor.y + 2, 2, LINE_HEIGHT - 5, MappedColor(UI_TEXT_COLOR));
        } else if (data->arbitrary_timer > s_cursor_limit) {
            data->arbitrary_timer = 0.0f;
        }
    } else {
        char buffer[32] = { 0 };
        snprintf(buffer, 32, "%llu", (long long unsigned int)(*value));
        Vector2 text_size = MeasureTextEx(FontAsset(), buffer, LINE_HEIGHT, 0);
        DrawTextEx(FontAsset(), buffer, (Vector2){ g_ui_cursor.x + (w/2) - (text_size.x/2), g_ui_cursor.y }, LINE_HEIGHT, 0, MappedColor(UI_TEXT_COLOR));
    }
    g_ui_cursor.y += LINE_HEIGHT;
    g_ui_cursor.x = 10;
    return ret;
}

BOOL UIDragUIntLabeled_(PersistantUIData* data, const char* label, uint32_t* value, uint32_t min, uint32_t max, uint32_t speed, size_t w) {
    UIDrawText(label);
    float xdif = MeasureTextEx(FontAsset(), label, LINE_HEIGHT, 0).x;
    UIMoveCursor(xdif + 5, -LINE_HEIGHT);
    return UIDragUInt_(data, value, min, max, speed, w - 5 - xdif);
}

BOOL UIDragInt_(PersistantUIData* data, int32_t* value, int32_t min, int32_t max, int32_t speed, size_t w) {
    if (g_textinput_data.submitted && g_textinput_data.data == data) {
        g_textinput_data.active = FALSE;
        g_textinput_data.data = NULL;
    }
    BOOL ret = FALSE;
    if (g_dragtext_owner == data && g_textinput_data.data != data) {
        int32_t prev = *value;
        *value = (int32_t)atoi(g_dragtext_buf);
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        if (prev != *value) ret = TRUE;
        g_dragtext_owner = NULL;
        g_was_ui_element_just_used = TRUE;
    }
    BOOL text_active = (g_dragtext_owner == data);
    DrawRectangle(g_ui_cursor.x, g_ui_cursor.y + 1, w, LINE_HEIGHT - 2,
        text_active ? MappedColor(UI_TEXT_INPUT_FOCUS_COLOR) : MappedColor(UI_DRAG_INT_COLOR));
    if (w > 20) {
        w -= 20;
        if (!text_active) {
            DrawTriangle(
                (Vector2){g_ui_cursor.x + 5 + w, g_ui_cursor.y + LINE_HEIGHT/2.0f + 2},
                (Vector2){g_ui_cursor.x + 10 + w, g_ui_cursor.y + LINE_HEIGHT - 2},
                (Vector2){g_ui_cursor.x + 15 + w, g_ui_cursor.y + LINE_HEIGHT/2.0f + 2},
                MappedColor(UI_TEXT_COLOR));
            DrawTriangle(
                (Vector2){g_ui_cursor.x + 15 + w, g_ui_cursor.y + LINE_HEIGHT/2.0f - 2},
                (Vector2){g_ui_cursor.x + 10 + w, g_ui_cursor.y + 2},
                (Vector2){g_ui_cursor.x + 5 + w, g_ui_cursor.y + LINE_HEIGHT/2.0f - 2},
                MappedColor(UI_TEXT_COLOR));
            if (InputButtonPressed(IK_MOUSELEFT) && CheckCollisionPointRec(
                GetMousePosition(),
                (Rectangle){ g_ui_cursor.x + g_ui_position.x + w, g_ui_position.y + g_ui_cursor.y, 20, LINE_HEIGHT/2.0f })) {
                if (*value < max) *value += 1;
                ret = TRUE;
            } else if (InputButtonPressed(IK_MOUSELEFT) && CheckCollisionPointRec(
                GetMousePosition(),
                (Rectangle){ g_ui_cursor.x + g_ui_position.x + w, g_ui_position.y + g_ui_cursor.y + LINE_HEIGHT/2.0f, 20, LINE_HEIGHT/2.0f })) {
                if (*value > min) *value -= 1;
                ret = TRUE;
            }
        }
    }
    Rectangle rect = {g_ui_cursor.x + g_ui_position.x, g_ui_cursor.y + g_ui_position.y + 2, w, LINE_HEIGHT - 4};
    if (!text_active && InputButtonPressed(IK_MOUSELEFT) && CheckCollisionPointRec(GetMousePosition(), rect)) {
        double now = GetTime();
        BOOL is_double = (g_last_click_owner == data) && (now - g_last_click_time < 0.3);
        g_last_click_owner = data;
        g_last_click_time = now;
        if (is_double) {
            snprintf(g_dragtext_buf, sizeof(g_dragtext_buf), "%d", *value);
            g_textinput_data = (TextInputData){
                g_dragtext_buf, sizeof(g_dragtext_buf), TRUE, data, strlen(g_dragtext_buf),
                (Vector2){rect.x, rect.y}, w, FALSE, TIM_INT
            };
            data->arbitrary_timer = 0.0f;
            g_dragtext_owner = data;
        } else {
            g_active_ui_element = data;
        }
    }
    if (!text_active && !g_ui_disabled && InputButtonPressed(IK_MOUSERIGHT) &&
        CheckCollisionPointRec(GetMousePosition(), rect)) {
        int32_t prev = *value;
        *value = 0;
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        if (prev != *value) ret = TRUE;
        g_was_ui_element_just_used = TRUE;
    }
    if (!text_active && g_active_ui_element == data) {
        int32_t prev = *value;
        if (GetMouseDelta().x * speed < min && GetMouseDelta().x * speed * -1 > *value)
            *value = 0;
        else
            *value += GetMouseDelta().x * speed;
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        if (prev != *value) ret = TRUE;
        g_was_ui_element_just_used = TRUE;
    }
    if (text_active) {
        Vector2 buf_size = MeasureTextEx(FontAsset(), g_dragtext_buf, LINE_HEIGHT, 0);
        float xstart = (w - buf_size.x) / 2.0f;
        if (xstart < 2.0f) xstart = 2.0f;
        DrawTextEx(FontAsset(), g_dragtext_buf, (Vector2){g_ui_cursor.x + xstart, g_ui_cursor.y}, LINE_HEIGHT, 0, MappedColor(UI_TEXT_COLOR));
        const float s_cursor_limit = 0.5f;
        data->arbitrary_timer += GetFrameTime();
        if (data->arbitrary_timer < s_cursor_limit / 2.0f) {
            char b[32] = { 0 };
            memcpy(b, g_dragtext_buf, g_textinput_data.cursor);
            float cx = g_ui_cursor.x + xstart + MeasureTextEx(FontAsset(), b, LINE_HEIGHT, 0).x;
            DrawRectangle(cx, g_ui_cursor.y + 2, 2, LINE_HEIGHT - 5, MappedColor(UI_TEXT_COLOR));
        } else if (data->arbitrary_timer > s_cursor_limit) {
            data->arbitrary_timer = 0.0f;
        }
    } else {
        char buffer[32] = { 0 };
        snprintf(buffer, 32, "%lld", (long long int)(*value));
        Vector2 text_size = MeasureTextEx(FontAsset(), buffer, LINE_HEIGHT, 0);
        DrawTextEx(FontAsset(), buffer, (Vector2){ g_ui_cursor.x + (w/2) - (text_size.x/2), g_ui_cursor.y }, LINE_HEIGHT, 0, MappedColor(UI_TEXT_COLOR));
    }
    g_ui_cursor.y += LINE_HEIGHT;
    g_ui_cursor.x = 10;
    return ret;
}

BOOL UIDragIntLabeled_(PersistantUIData* data, const char* label, int32_t* value, int32_t min, int32_t max, int32_t speed, size_t w) {
    UIDrawText(label);
    float xdif = MeasureTextEx(FontAsset(), label, LINE_HEIGHT, 0).x;
    UIMoveCursor(xdif + 5, -LINE_HEIGHT);
    return UIDragInt_(data, value, min, max, speed, w - 5 - xdif);
}

BOOL UIDragSize_(PersistantUIData* data, size_t* value, size_t min, size_t max, size_t speed, size_t w) {
    if (g_textinput_data.submitted && g_textinput_data.data == data) {
        g_textinput_data.active = FALSE;
        g_textinput_data.data = NULL;
    }
    BOOL ret = FALSE;
    if (g_dragtext_owner == data && g_textinput_data.data != data) {
        size_t prev = *value;
        *value = (size_t)strtoul(g_dragtext_buf, NULL, 10);
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        if (prev != *value) ret = TRUE;
        g_dragtext_owner = NULL;
        g_was_ui_element_just_used = TRUE;
    }
    BOOL text_active = (g_dragtext_owner == data);
    DrawRectangle(g_ui_cursor.x, g_ui_cursor.y + 1, w, LINE_HEIGHT - 2,
        g_ui_disabled ? MappedColor(UI_BOX_DISABLED) :
        text_active ? MappedColor(UI_TEXT_INPUT_FOCUS_COLOR) : MappedColor(UI_DRAG_INT_COLOR));
    if (w > 20) {
        w -= 20;
        if (!text_active) {
            DrawTriangle(
                (Vector2){g_ui_cursor.x + 5 + w, g_ui_cursor.y + LINE_HEIGHT/2.0f + 2},
                (Vector2){g_ui_cursor.x + 10 + w, g_ui_cursor.y + LINE_HEIGHT - 2},
                (Vector2){g_ui_cursor.x + 15 + w, g_ui_cursor.y + LINE_HEIGHT/2.0f + 2},
                g_ui_disabled ? MappedColor(UI_TEXT_DISABLED) : MappedColor(UI_TEXT_COLOR));
            DrawTriangle(
                (Vector2){g_ui_cursor.x + 15 + w, g_ui_cursor.y + LINE_HEIGHT/2.0f - 2},
                (Vector2){g_ui_cursor.x + 10 + w, g_ui_cursor.y + 2},
                (Vector2){g_ui_cursor.x + 5 + w, g_ui_cursor.y + LINE_HEIGHT/2.0f - 2},
                g_ui_disabled ? MappedColor(UI_TEXT_DISABLED) : MappedColor(UI_TEXT_COLOR));
            if (!g_ui_disabled && InputButtonPressed(IK_MOUSELEFT) && CheckCollisionPointRec(
                GetMousePosition(),
                (Rectangle){ g_ui_cursor.x + g_ui_position.x + w, g_ui_position.y + g_ui_cursor.y, 20, LINE_HEIGHT/2.0f })) {
                if (*value < max) *value += 1;
                ret = TRUE;
            } else if (!g_ui_disabled && InputButtonPressed(IK_MOUSELEFT) && CheckCollisionPointRec(
                GetMousePosition(),
                (Rectangle){ g_ui_cursor.x + g_ui_position.x + w, g_ui_position.y + g_ui_cursor.y + LINE_HEIGHT/2.0f, 20, LINE_HEIGHT/2.0f })) {
                if (*value > min) *value -= 1;
                ret = TRUE;
            }
        }
    }
    Rectangle rect = {g_ui_cursor.x + g_ui_position.x, g_ui_cursor.y + g_ui_position.y + 2, w, LINE_HEIGHT - 4};
    if (!text_active && !g_ui_disabled && InputButtonPressed(IK_MOUSELEFT) && CheckCollisionPointRec(GetMousePosition(), rect)) {
        double now = GetTime();
        BOOL is_double = (g_last_click_owner == data) && (now - g_last_click_time < 0.3);
        g_last_click_owner = data;
        g_last_click_time = now;
        if (is_double) {
            snprintf(g_dragtext_buf, sizeof(g_dragtext_buf), "%u", (uint32_t)(*value));
            g_textinput_data = (TextInputData){
                g_dragtext_buf, sizeof(g_dragtext_buf), TRUE, data, strlen(g_dragtext_buf),
                (Vector2){rect.x, rect.y}, w, FALSE, TIM_UINT
            };
            data->arbitrary_timer = 0.0f;
            g_dragtext_owner = data;
        } else {
            g_active_ui_element = data;
        }
    }
    if (!text_active && !g_ui_disabled && InputButtonPressed(IK_MOUSERIGHT) &&
        CheckCollisionPointRec(GetMousePosition(), rect)) {
        size_t prev = *value;
        *value = 0;
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        if (prev != *value) ret = TRUE;
        g_was_ui_element_just_used = TRUE;
    }
    if (!text_active && g_active_ui_element == data) {
        size_t prev = *value;
        if (GetMouseDelta().x * speed < 0 && GetMouseDelta().x * speed * -1 > *value)
            *value = 0;
        else
            *value += GetMouseDelta().x * speed;
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        if (prev != *value) ret = TRUE;
        g_was_ui_element_just_used = TRUE;
    }
    if (text_active) {
        Vector2 buf_size = MeasureTextEx(FontAsset(), g_dragtext_buf, LINE_HEIGHT, 0);
        float xstart = (w - buf_size.x) / 2.0f;
        if (xstart < 2.0f) xstart = 2.0f;
        DrawTextEx(FontAsset(), g_dragtext_buf, (Vector2){g_ui_cursor.x + xstart, g_ui_cursor.y}, LINE_HEIGHT, 0, MappedColor(UI_TEXT_COLOR));
        const float s_cursor_limit = 0.5f;
        data->arbitrary_timer += GetFrameTime();
        if (data->arbitrary_timer < s_cursor_limit / 2.0f) {
            char b[32] = { 0 };
            memcpy(b, g_dragtext_buf, g_textinput_data.cursor);
            float cx = g_ui_cursor.x + xstart + MeasureTextEx(FontAsset(), b, LINE_HEIGHT, 0).x;
            DrawRectangle(cx, g_ui_cursor.y + 2, 2, LINE_HEIGHT - 5, MappedColor(UI_TEXT_COLOR));
        } else if (data->arbitrary_timer > s_cursor_limit) {
            data->arbitrary_timer = 0.0f;
        }
    } else {
        char buffer[32] = { 0 };
        snprintf(buffer, 32, "%llu", (long long unsigned int)(*value));
        Vector2 text_size = MeasureTextEx(FontAsset(), buffer, LINE_HEIGHT, 0);
        DrawTextEx(FontAsset(), buffer, (Vector2){ g_ui_cursor.x + (w/2) - (text_size.x/2), g_ui_cursor.y }, LINE_HEIGHT, 0, g_ui_disabled ? MappedColor(UI_TEXT_DISABLED) : MappedColor(UI_TEXT_COLOR));
    }
    g_ui_cursor.y += LINE_HEIGHT;
    g_ui_cursor.x = 10;
    return ret;
}

BOOL UIDragSizeLabeled_(PersistantUIData* data, const char* label, size_t* value, size_t min, size_t max, size_t speed, size_t w) {
    UIDrawText(label);
    float xdif = MeasureTextEx(FontAsset(), label, LINE_HEIGHT, 0).x;
    UIMoveCursor(xdif + 5, -LINE_HEIGHT);
    return UIDragSize_(data, value, min, max, speed, w - 5 - xdif);
}

BOOL UIButton(const char* label, size_t w) {
    Vector2 text_size = MeasureTextEx(FontAsset(), label, LINE_HEIGHT, 0);
    float button_width = w < text_size.x + 20 ? text_size.x + 20 : w;
    Color color = MappedColor(PANEL_BTN_BG_COLOR);
    BOOL clicked = FALSE;
    if (!InputsBlocked() && !g_ui_disabled && CheckCollisionPointRec(
            GetMousePosition(),
            (Rectangle){g_ui_cursor.x + g_ui_position.x, g_ui_cursor.y + g_ui_position.y + 1, button_width, LINE_HEIGHT - 2})) {
        color = MappedColor(PANEL_BTN_HVR_COLOR);
        if (InputButtonDown(IK_MOUSELEFT)) color = MappedColor(PANEL_BTN_PRS_COLOR);
        clicked = InputButtonPressed(IK_MOUSELEFT);
    }
    if (clicked) g_was_ui_element_just_used = TRUE;
    DrawRectangle(g_ui_cursor.x, g_ui_cursor.y + 1, button_width, LINE_HEIGHT - 2, 
                  g_ui_disabled ? MappedColor(UI_BOX_DISABLED) : color);
    Vector2 texpos = g_ui_cursor;
    texpos.x += (button_width - text_size.x) / 2.0f;
    DrawTextEx(FontAsset(), label, texpos, LINE_HEIGHT, 0, 
               g_ui_disabled ? MappedColor(UI_TEXT_DISABLED) : MappedColor(UI_TEXT_COLOR));
    g_ui_cursor.y += LINE_HEIGHT;
    g_ui_cursor.x = 10;
    return clicked;
}

void UIPopup(Popup* popup) {
    g_popup = popup;
    g_popup_origin = popup;
}

float UITextWidth(const char* text, ...) {
    va_list args;
    va_start(args, text);
    vsnprintf(g_ui_text_buffer, MAX_LINE_WIDTH - 1, text, args);
    return MeasureTextEx(FontAsset(), g_ui_text_buffer, LINE_HEIGHT, 0).x;
}

float UITextHeight(const char* text, ...) {
    va_list args;
    va_start(args, text);
    vsnprintf(g_ui_text_buffer, MAX_LINE_WIDTH - 1, text, args);
    return MeasureTextEx(FontAsset(), g_ui_text_buffer, LINE_HEIGHT, 0).y;
}

void UIDivider(size_t w) {
    DrawRectangle(g_ui_cursor.x, g_ui_cursor.y + (LINE_HEIGHT/2.0f) - 1, w, 2, MappedColor(UI_DIVIDER_COLOR));
    g_ui_cursor.y += LINE_HEIGHT;
    g_ui_cursor.x = 10;
}

void UIDividerLabeled(size_t w, const char* label) {
    float t = (w - 20 - UITextWidth(label)) * 0.75f;
    float tw = (w - 20 - UITextWidth(label)) * 0.25f;
    DrawRectangleGradientH(UIGetCursor().x + t, UIGetCursor().y + (LINE_HEIGHT/2.0f) - 1, tw, 2, MappedColor(UI_DIVIDER_COLOR), (Color){ 0, 0, 0, 0 });
    UIDivider(t);
    UIMoveCursor(w - UITextWidth(label) - 20, -20);
    UIDrawItalicText(label);
}

void UIDropList_(PersistantUIData* data, const char* label, size_t width, size_t num_items, char** items, SelectFunction func, size_t selected) {
    float clickwidth = UITextWidth(label) + 25;
    clickwidth = clickwidth < width ? width : clickwidth;
    if (CheckCollisionPointRec(
            GetMousePosition(),
            (Rectangle){g_ui_cursor.x + g_ui_position.x, g_ui_cursor.y + g_ui_position.y + 2, clickwidth, LINE_HEIGHT - 4})) {
        DrawRectangle(
            g_ui_cursor.x, g_ui_cursor.y + 2, clickwidth, LINE_HEIGHT - 4,
            InputButtonDown(IK_MOUSELEFT) ? MappedColor(PANEL_BTN_PRS_COLOR) : MappedColor(PANEL_BTN_HVR_COLOR));
        if (InputButtonPressed(IK_MOUSELEFT)) data->arbitrary_bool = !data->arbitrary_bool;
    }
    if (data->arbitrary_bool) {
        DrawTriangle(
            (Vector2){g_ui_cursor.x + 5, g_ui_cursor.y + LINE_HEIGHT/2.0f - 5},
            (Vector2){g_ui_cursor.x + 10, g_ui_cursor.y + LINE_HEIGHT/2.0f + 5},
            (Vector2){g_ui_cursor.x + 15, g_ui_cursor.y + LINE_HEIGHT/2.0f - 5},
            MappedColor(UI_TEXT_COLOR));
    } else {
        DrawTriangle(
            (Vector2){g_ui_cursor.x + 5, g_ui_cursor.y + LINE_HEIGHT/2.0f - 5},
            (Vector2){g_ui_cursor.x + 5, g_ui_cursor.y + LINE_HEIGHT/2.0f + 5},
            (Vector2){g_ui_cursor.x + 15, g_ui_cursor.y + LINE_HEIGHT/2.0f},
            MappedColor(UI_TEXT_COLOR));
    }
    g_ui_cursor.x += 20;
    UIDrawText(label);
    if (data->arbitrary_bool) {
        float top = g_ui_cursor.y;
        for (size_t i = 0; i < num_items; i++) {
            g_ui_cursor.x += 30;
            if (func && CheckCollisionPointRec(
                GetMousePosition(),
                (Rectangle){g_ui_cursor.x + g_ui_position.x, g_ui_cursor.y + g_ui_position.y + 2, clickwidth - 30, LINE_HEIGHT - 4})) {
                DrawRectangle(
                    g_ui_cursor.x, g_ui_cursor.y + 2, clickwidth - 30, LINE_HEIGHT - 4,
                    InputButtonDown(IK_MOUSELEFT) ? MappedColor(PANEL_BTN_PRS_COLOR) : MappedColor(PANEL_BTN_HVR_COLOR));
                if (InputButtonPressed(IK_MOUSELEFT)) {
                    func(i);
                    g_was_ui_element_just_used = TRUE;
                }
            }
            if (selected == i) {
                DrawRectangle(g_ui_cursor.x, g_ui_cursor.y + 2, clickwidth - 30, LINE_HEIGHT - 4, MappedColor(PANEL_BTN_SLC_COLOR));
            }
            UIDrawText(items[i]);
        }
        if (num_items > 0) DrawRectangle(g_ui_cursor.x + 20, top + 5, 2, num_items*LINE_HEIGHT - 5, MappedColor(PANEL_BTN_HVR_COLOR));
    }
}

void UIDropdownMenu_(PersistantUIData* data, size_t width, size_t num_items, char** items, DropdownSelectFunction func, void* param) {
    data->arbitrary_counter = func(param, (size_t)-1, FALSE);
    Vector2 text_size = MeasureTextEx(FontAsset(), items[data->arbitrary_counter], LINE_HEIGHT, 0);
    float button_width = width < text_size.x + 20 ? text_size.x + 20 : width;
    Color color = MappedColor(PANEL_BTN_BG_COLOR);
    BOOL clicked = FALSE;
    if (CheckCollisionPointRec(
            GetMousePosition(),
            (Rectangle){g_ui_cursor.x + g_ui_position.x, g_ui_cursor.y + g_ui_position.y + 1, button_width, LINE_HEIGHT - 2})) {
        color = MappedColor(PANEL_BTN_HVR_COLOR);
        if (InputButtonDown(IK_MOUSELEFT)) color = MappedColor(PANEL_BTN_PRS_COLOR);
        clicked = InputButtonPressed(IK_MOUSELEFT);
    }
    if (clicked) g_was_ui_element_just_used = TRUE;
    DrawRectangle(g_ui_cursor.x, g_ui_cursor.y + 1, button_width, LINE_HEIGHT - 2, color);
    Vector2 texpos = g_ui_cursor;
    texpos.x += (button_width - text_size.x) / 2.0f;
    DrawTextEx(FontAsset(), items[data->arbitrary_counter], texpos, LINE_HEIGHT, 0, MappedColor(UI_TEXT_COLOR));
    if (clicked) {
        g_dropdownmenu_data = (DropdownMenuData) {
            data, items, num_items, TRUE, button_width, param, func,
            (Vector2) { g_ui_cursor.x + g_ui_position.x, g_ui_cursor.y + g_ui_position.y }
        };
    }
    g_ui_cursor.y += LINE_HEIGHT;
    g_ui_cursor.x = 10;
}

BOOL UITextInput_(PersistantUIData* data, const char* label, char* buffer, size_t size, size_t width, BOOL noclear) {
    BOOL retval = FALSE;
    const float s_cursor_limit = 0.5f;
    if (label) {
        DrawTextEx(FontAsset(), label, g_ui_cursor, LINE_HEIGHT, 0, MappedColor(UI_TEXT_COLOR));
    }
    Vector2 text_size = label ? MeasureTextEx(FontAsset(), label, LINE_HEIGHT, 0) : (Vector2){ 0, LINE_HEIGHT };
    float labeloffset = label ? 10.0f : 0;
    float box_width = width - text_size.x - labeloffset;
    if (g_textinput_data.submitted && g_textinput_data.data == data) {
        if (noclear) {
            g_textinput_data = (TextInputData){
                buffer, size, TRUE, data, strlen(buffer), 
                (Vector2){g_ui_cursor.x + g_ui_position.x, g_ui_cursor.y + g_ui_position.y}, box_width, FALSE, TIM_ALPHANUMERIC};
        } else {
            g_textinput_data.data = NULL;
        }
        retval = TRUE;
    }
    if (box_width < 0) return retval;
    g_ui_cursor.x += text_size.x + labeloffset;
    BOOL g_ui_disabled_bak = g_ui_disabled;
    if (size == 0) DisableUI();
    Color box_color = g_textinput_data.data == data ? MappedColor(UI_TEXT_INPUT_FOCUS_COLOR) : MappedColor(UI_TEXT_INPUT_BG_COLOR);
    Color text_color = MappedColor(UI_TEXT_COLOR);
    if (g_ui_disabled) {
        box_color.a = (unsigned char)(box_color.a * 180 / 255);
        text_color.a = (unsigned char)(text_color.a * 180 / 255);
    }
    DrawRectangleRec((Rectangle){ g_ui_cursor.x, g_ui_cursor.y, box_width, LINE_HEIGHT - 2 }, box_color);
    PushUIScissor((Rectangle){ g_ui_cursor.x + g_ui_origin.x, g_ui_cursor.y + g_ui_origin.y, box_width, LINE_HEIGHT - 2 });
    Vector2 buffer_text_size = MeasureTextEx(FontAsset(), buffer, LINE_HEIGHT, 0);
    float xstart = 2.0f;
    if (g_textinput_data.data == data && buffer_text_size.x > box_width) xstart -= (buffer_text_size.x - box_width) + 4.0f;
    DrawTextEx(FontAsset(), buffer, (Vector2){ g_ui_cursor.x + xstart, g_ui_cursor.y }, LINE_HEIGHT, 0, text_color);
    if (g_textinput_data.data == data) {
        data->arbitrary_timer += GetFrameTime();
        if (data->arbitrary_timer < s_cursor_limit / 2.0f) {
            char b[1024] = { 0 };
            memcpy(b, buffer, g_textinput_data.cursor);
            DrawRectangle(g_ui_cursor.x + MeasureTextEx(FontAsset(), b, LINE_HEIGHT, 0).x + xstart, g_ui_cursor.y + 2, 2, LINE_HEIGHT - 5, text_color);
        } else if (data->arbitrary_timer > s_cursor_limit) data->arbitrary_timer = 0.0f;
    }
    PopUIScissor();
    if (!g_ui_disabled && CheckCollisionPointRec(
            GetMousePosition(),
            (Rectangle){g_ui_cursor.x + g_ui_position.x, g_ui_cursor.y + g_ui_position.y, box_width, LINE_HEIGHT - 2})) {
        if (InputButtonPressed(IK_MOUSELEFT)) {
            g_textinput_data = (TextInputData){
                buffer, size, TRUE, data, strlen(buffer), 
                (Vector2){g_ui_cursor.x + g_ui_position.x, g_ui_cursor.y + g_ui_position.y}, box_width, FALSE, TIM_ALPHANUMERIC};
            data->arbitrary_timer = 0.0f;
        }
    }
    g_ui_cursor.x = 10;
    g_ui_cursor.y += LINE_HEIGHT;
    g_ui_disabled = g_ui_disabled_bak;
    return retval;
}

void UIFloatingDropdown_(PersistantUIData* data, size_t width, Vector2 origin, size_t num_items, char** items, DropdownSelectFunction func, void* param) {
    data->arbitrary_counter = -1;
    g_dropdownmenu_data = (DropdownMenuData) { data, items, num_items, TRUE, width, param, func, origin };
}

BOOL UIDropdownSection_(PersistantUIData* data, const char* label, size_t width, DrawSectionFunction func, void* param) {
    BOOL changed = FALSE;
    if (CheckCollisionPointRec(
            GetMousePosition(),
            (Rectangle){g_ui_cursor.x + g_ui_position.x, g_ui_cursor.y + g_ui_position.y + 2, width, LINE_HEIGHT - 4})) {
        DrawRectangle(
            g_ui_cursor.x, g_ui_cursor.y + 2, width, LINE_HEIGHT - 4,
            InputButtonDown(IK_MOUSELEFT) ? MappedColor(PANEL_BTN_PRS_COLOR) : MappedColor(PANEL_BTN_HVR_COLOR));
        if (InputButtonPressed(IK_MOUSELEFT)) {
            data->arbitrary_bool = !data->arbitrary_bool;
            changed = TRUE;
        }
    }
    if (data->arbitrary_bool) {
        DrawTriangle(
            (Vector2){ UIGetCursor().x + 10.0f + (10.0f / 2.0f), UIGetCursor().y + 10.0f - (10.0f / 2.0f) },
            (Vector2){ UIGetCursor().x + 10.0f - (10.0f / 2.0f), UIGetCursor().y + 10.0f - (10.0f / 2.0f) },
            (Vector2){ UIGetCursor().x + 10.0f, UIGetCursor().y + 10.0f + (10.0f / 2.0f) },
            MappedColor(UI_TEXT_COLOR));
    } else {
        DrawTriangle(
            (Vector2){ UIGetCursor().x + 10.0f - (10.0f / 2.0f), UIGetCursor().y + 10.0f - (10.0f / 2.0f) },
            (Vector2){ UIGetCursor().x + 10.0f - (10.0f / 2.0f), UIGetCursor().y + 10.0f + (10.0f / 2.0f) },
            (Vector2){ UIGetCursor().x + 10.0f + (10.0f / 2.0f), UIGetCursor().y + 10.0f },
            MappedColor(UI_TEXT_COLOR));
    }
    g_ui_cursor.x += 20;
    UIDrawText(label);
    if (data->arbitrary_bool) func(width, param);
    return changed;
}

void DisableUI() {
    g_ui_disabled = TRUE;
}

void EnableUI() {
    g_ui_disabled = FALSE;
}

void PausePreRender() {
    if (!g_ui_drawing) return;
    UnbindUIState();
    g_ui_paused = TRUE;
}

void ResumePreRender() {
    if (!g_ui_paused) return;
    g_ui_paused = FALSE;
    BindUIState();
}

void ToggleFullScreen() {
    if (g_current_prendered_ui == g_fullscreen_ui) g_fullscreen_ui = NULL;
    else g_fullscreen_ui = g_current_prendered_ui;
}

BOOL IsFullScreen() {
    return g_fullscreen_ui != NULL;
}

static void LoadUIConfigHelper(UI** current, size_t* index, ARRLIST_Panel panelbank) {
	UIConfig conf = g_ui_config.data[*index];
	if (*current == NULL) *current = GenerateUI();
	if (conf.left || conf.right) {
		(*current)->divide = conf.divide;
		(*current)->vertical = conf.vertical;
		if (conf.left) {
			*index += 1;
			LoadUIConfigHelper((UI**)&((*current)->left), index, panelbank);
		}
		if (conf.right) {
			*index += 1;
			LoadUIConfigHelper((UI**)&((*current)->right), index, panelbank);
		}
	} else {
		for (size_t i = 0; i < panelbank.size; i++) {
			if (strcmp(panelbank.data[i].name, conf.name) == 0) {
				ARRLIST_Panel_add(&((*current)->panels), panelbank.data[i]);
				if (conf.vine) {
					*index += 1;
					LoadUIConfigHelper(current, index, panelbank);
				}
				return;
			}	
		}
		logwarn("Unable to find and set up matching panel \"%s\"", conf.name);
	}
}

void LoadUIConfig(UI** ui, ARRLIST_Panel panelbank) {
    FILE* file = fopen(".uiconf", "rb");
    if (file) {
        fseek(file, 0, SEEK_END);
        long filesize = ftell(file);
        if ((filesize - sizeof(size_t)) % sizeof(UIConfig) == 0) {
            rewind(file);
            size_t count = 0;
            fread(&count, 1, sizeof(size_t), file);
            if (count > 0) {
                if ((size_t)filesize == sizeof(size_t) + count * sizeof(UIConfig)) {
                    ARRLIST_UIConfig_clear(&g_ui_config);
                    for (size_t i = 0; i < count; i++) {
                        UIConfig conf = { 0 };
                        fread(&conf, 1, sizeof(UIConfig), file);
                        ARRLIST_UIConfig_add(&g_ui_config, conf);
                    }
                    logtrace("Successfully loaded ui config");
                } else {
                    logwarn("Desynced panel count detected - falling back to default config");
                }
            } else {
                logwarn("No configured panels detected - falling back to default config");
            }
        } else {
            logwarn("Existing ui config is invalid - falling back to default config");
        }
        fclose(file);
    } else {
        logtrace("Unable to detect existing ui config - using default config");
    }
	size_t i = 0;
    LoadUIConfigHelper(ui, &i, panelbank);
    SetPrimaryUI(*ui);
}

static void CopyUIDividersToConfigHelper(UI* ui, size_t* index) {
    g_ui_config.data[*index].divide = ui->divide;
    if (ui->left) {
        *index += 1;
        CopyUIDividersToConfigHelper(GetLeftUI(ui), index);
    }
    if (ui->right) {
        *index += 1;
        CopyUIDividersToConfigHelper(GetRightUI(ui), index);
    }
    if (ui->panels.size > 0 ) *index += ui->panels.size - 1;
}

void SaveUIConfig(UI* ui) {
    size_t i = 0;
    CopyUIDividersToConfigHelper(ui, &i);
    FILE *file = fopen(".uiconf", "wb");
    if (file) {
        fwrite(&g_ui_config.size, 1, sizeof(size_t), file);
        for (size_t i = 0; i < g_ui_config.size; i++)
            fwrite(&(g_ui_config.data[i]), 1, sizeof(UIConfig), file);
        fclose(file);
    }
}

ARRLIST_UIConfig* GetUIConfig() {
    return &g_ui_config;
}

void SetUIConfig(ARRLIST_UIConfig* config) {
	ARRLIST_UIConfig_clear(&g_ui_config);
    for (size_t i = 0; i < config->size; i++) {
        ARRLIST_UIConfig_add(&g_ui_config, config->data[i]);
    }
    g_reset_ui = TRUE;
}

void RefreshUI(UI** ui, ARRLIST_Panel panelbank) {
    if (g_reset_ui){
        g_reset_ui = FALSE;
        WipeUI(*ui);
        *ui = NULL;
        size_t i = 0;
        LoadUIConfigHelper(ui, &i, panelbank);
        SetPrimaryUI(*ui);
        ResizeUI();
    }
}
