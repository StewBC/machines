#pragma once

#include <stdbool.h>
#include <stddef.h>

/*
 * Mac .app launch support. Other platforms are no-ops.
 *
 * Call platform_host_begin once, before option parsing. A Finder launch of a
 * bundle (no terminal) appends stderr to ~/Library/Logs/<log_name>.log.
 * A terminal launch, including a symlink into the bundle, leaves stderr alone.
 */
void platform_host_begin(const char *log_name);

/* True only after begin() saw a bundle opened with no terminal. */
bool platform_host_bundle_gui_launch(void);

/*
 * ~/Library/Application Support/<app_name>/<ini_name>. Creates the directory.
 * Succeeds only for a bundle GUI launch, so a terminal run keeps ./<ini>.
 */
bool platform_host_bundle_default_ini(
    const char *app_name,
    const char *ini_name,
    char *out,
    size_t out_size);

/* Contents/Resources when this executable lives inside a .app. */
bool platform_host_resources_dir(char *out, size_t out_size);

/*
 * Application menu: Show Log, Show Samples, Install Command Line Tool.
 * Call after the SDL window exists. No-op outside a bundle.
 * cli_tool is the name linked into /usr/local/bin. am65 is linked too when
 * it sits beside this executable.
 */
void platform_host_install_app_menu(const char *display_name, const char *cli_tool);
