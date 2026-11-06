#include "platform_host.h"

void platform_host_begin(const char *log_name)
{
    (void)log_name;
}

bool platform_host_bundle_gui_launch(void)
{
    return false;
}

bool platform_host_bundle_default_ini(
    const char *app_name,
    const char *ini_name,
    char *out,
    size_t out_size)
{
    (void)app_name;
    (void)ini_name;
    (void)out;
    (void)out_size;
    return false;
}

bool platform_host_resources_dir(char *out, size_t out_size)
{
    (void)out;
    (void)out_size;
    return false;
}

void platform_host_install_app_menu(const char *display_name, const char *cli_tool)
{
    (void)display_name;
    (void)cli_tool;
}
