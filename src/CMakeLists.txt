SET(SOURCES screens/ui_main.c
    screens/ui_keypad.c
    screens/ui_denied.c
    screens/ui_control.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_kubernetes_logo_without_workmark_png.c
    images/ui_img_unlock_new2_png.c
    images/ui_img_access_denied_png.c
    images/ui_img_arrow_png_png.c
    images/ui_img_lock_new2_png.c
    images/ui_img_play2_png.c
    images/ui_img_completed_png.c
    images/ui_img_failed_png.c)

add_library(ui ${SOURCES})
