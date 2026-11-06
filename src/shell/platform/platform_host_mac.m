#import "platform_host.h"

#import <AppKit/AppKit.h>
#import <limits.h>
#import <mach-o/dyld.h>
#import <stdio.h>
#import <string.h>
#import <unistd.h>

static bool g_gui_launch = false;
static char g_log_path[PATH_MAX];

static NSString *shell_single_quote(NSString *text);

static bool copy_executable_path(char *out, size_t out_size)
{
    char raw[PATH_MAX];
    char resolved[PATH_MAX];
    uint32_t size = (uint32_t)sizeof(raw);

    if (out == NULL || out_size == 0) {
        return false;
    }
    if (_NSGetExecutablePath(raw, &size) != 0) {
        return false;
    }
    if (realpath(raw, resolved) == NULL) {
        return false;
    }
    if (strlen(resolved) + 1 > out_size) {
        return false;
    }
    memcpy(out, resolved, strlen(resolved) + 1);
    return true;
}

static bool executable_in_bundle(const char *exe_path)
{
    return exe_path != NULL && strstr(exe_path, ".app/Contents/MacOS/") != NULL;
}

/* Keep the on-disk log from growing without bound. Show Log reads the tail. */
static void trim_log_tail(const char *path)
{
    NSString *file = [NSString stringWithUTF8String:path];
    NSData *data = [NSData dataWithContentsOfFile:file];
    NSData *tail;

    if (data.length < 256 * 1024) {
        return;
    }
    tail = [data subdataWithRange:NSMakeRange(data.length - 64 * 1024, 64 * 1024)];
    [tail writeToFile:file atomically:YES];
}

static bool resources_dir_from_exe(const char *exe_path, char *out, size_t out_size)
{
    char copy[PATH_MAX];
    char *slash;
    size_t len;

    if (!executable_in_bundle(exe_path)) {
        return false;
    }
    len = strlen(exe_path);
    if (len + 1 > sizeof(copy)) {
        return false;
    }
    memcpy(copy, exe_path, len + 1);
    /* .../Name.app/Contents/MacOS/name -> .../Name.app/Contents */
    slash = strrchr(copy, '/');
    if (slash == NULL) {
        return false;
    }
    *slash = '\0';
    slash = strrchr(copy, '/');
    if (slash == NULL) {
        return false;
    }
    *slash = '\0';
    if (snprintf(out, out_size, "%s/Resources", copy) >= (int)out_size) {
        return false;
    }
    return true;
}

void platform_host_begin(const char *log_name)
{
    char exe[PATH_MAX];
    const char *home;
    FILE *log_file;

    g_gui_launch = false;
    g_log_path[0] = '\0';
    if (log_name == NULL || log_name[0] == '\0') {
        return;
    }
    if (!copy_executable_path(exe, sizeof(exe)) || !executable_in_bundle(exe)) {
        return;
    }
    if (isatty(STDERR_FILENO)) {
        return;
    }

    home = NSHomeDirectory().UTF8String;
    if (home == NULL) {
        return;
    }
    if (snprintf(g_log_path, sizeof(g_log_path), "%s/Library/Logs/%s.log", home, log_name) >=
        (int)sizeof(g_log_path)) {
        g_log_path[0] = '\0';
        return;
    }
    trim_log_tail(g_log_path);
    log_file = freopen(g_log_path, "a", stderr);
    if (log_file == NULL) {
        g_log_path[0] = '\0';
        return;
    }
    setvbuf(stderr, NULL, _IOLBF, 0);
    fprintf(stderr, "\n---- %s ----\n", log_name);
    g_gui_launch = true;
}

bool platform_host_bundle_gui_launch(void)
{
    return g_gui_launch;
}

bool platform_host_bundle_default_ini(
    const char *app_name,
    const char *ini_name,
    char *out,
    size_t out_size)
{
    NSString *dir;
    NSString *path;

    if (!g_gui_launch || app_name == NULL || ini_name == NULL || out == NULL || out_size == 0) {
        return false;
    }
    dir = [NSHomeDirectory() stringByAppendingPathComponent:
        [NSString stringWithFormat:@"Library/Application Support/%s", app_name]];
    if (![[NSFileManager defaultManager] createDirectoryAtPath:dir
                                   withIntermediateDirectories:YES
                                                    attributes:nil
                                                         error:nil]) {
        return false;
    }
    path = [dir stringByAppendingPathComponent:[NSString stringWithUTF8String:ini_name]];
    if (path.UTF8String == NULL || strlen(path.UTF8String) + 1 > out_size) {
        return false;
    }
    memcpy(out, path.UTF8String, strlen(path.UTF8String) + 1);
    return true;
}

bool platform_host_resources_dir(char *out, size_t out_size)
{
    char exe[PATH_MAX];

    if (!copy_executable_path(exe, sizeof(exe))) {
        return false;
    }
    return resources_dir_from_exe(exe, out, out_size);
}

@interface MachinesAppMenu : NSObject
@property(copy) NSString *displayName;
@property(copy) NSString *cliTool;
- (void)showLog:(id)sender;
- (void)showSamples:(id)sender;
- (void)installCli:(id)sender;
@end

@implementation MachinesAppMenu

- (void)showLog:(id)sender
{
    NSString *text;
    NSRect frame;
    NSWindow *window;
    NSScrollView *scroll;
    NSTextView *view;
    (void)sender;

    if (g_log_path[0] == '\0') {
        text = @"This launch is using the terminal, so there is no log file.";
    } else {
        NSString *path = [NSString stringWithUTF8String:g_log_path];
        NSData *data = [NSData dataWithContentsOfFile:path];
        if (data.length > 64 * 1024) {
            data = [data subdataWithRange:NSMakeRange(data.length - 64 * 1024, 64 * 1024)];
        }
        text = data != nil ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : nil;
        if (text == nil) {
            text = @"The log is empty.";
        }
    }

    frame = NSMakeRect(0, 0, 720, 480);
    window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                    NSWindowStyleMaskResizable
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    window.title = [NSString stringWithFormat:@"%@ Log", self.displayName ?: @"Log"];
    window.releasedWhenClosed = NO;
    scroll = [[NSScrollView alloc] initWithFrame:frame];
    scroll.hasVerticalScroller = YES;
    scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    view = [[NSTextView alloc] initWithFrame:frame];
    view.editable = NO;
    view.richText = NO;
    view.font = [NSFont fontWithName:@"Menlo" size:12] ?: [NSFont userFixedPitchFontOfSize:12];
    view.string = text;
    scroll.documentView = view;
    window.contentView = scroll;
    [window center];
    [window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (void)showSamples:(id)sender
{
    char resources[PATH_MAX];
    NSString *samples;
    (void)sender;

    if (!platform_host_resources_dir(resources, sizeof(resources))) {
        return;
    }
    samples = [[NSString stringWithUTF8String:resources] stringByAppendingPathComponent:@"samples"];
    if (![[NSFileManager defaultManager] fileExistsAtPath:samples]) {
        samples = [NSString stringWithUTF8String:resources];
    }
    [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:samples]];
}

- (void)installCli:(id)sender
{
    char exe[PATH_MAX];
    NSString *exePath;
    NSString *binDir;
    NSString *am65;
    NSMutableString *shell;
    NSAppleScript *script;
    NSDictionary *error = nil;
    NSAlert *alert;
    (void)sender;

    if (!copy_executable_path(exe, sizeof(exe)) || self.cliTool.length == 0) {
        return;
    }
    exePath = [NSString stringWithUTF8String:exe];
    binDir = [exePath stringByDeletingLastPathComponent];
    am65 = [binDir stringByAppendingPathComponent:@"am65"];
    shell = [NSMutableString stringWithString:@"do shell script \"mkdir -p /usr/local/bin"];
    [shell appendFormat:@" && ln -sfn '%@' '/usr/local/bin/%@'",
        shell_single_quote(exePath), self.cliTool];
    if ([[NSFileManager defaultManager] isExecutableFileAtPath:am65]) {
        [shell appendFormat:@" && ln -sfn '%@' '/usr/local/bin/am65'", shell_single_quote(am65)];
    }
    [shell appendString:@"\" with administrator privileges"];

    script = [[NSAppleScript alloc] initWithSource:shell];
    alert = [[NSAlert alloc] init];
    if ([script executeAndReturnError:&error] == nil) {
        alert.messageText = @"Could not install the command line tool";
        alert.informativeText = error[NSAppleScriptErrorMessage] ?: @"The install was cancelled.";
    } else {
        alert.messageText = @"Command line tool installed";
        alert.informativeText = [NSString stringWithFormat:
            @"%@ is now on your PATH as /usr/local/bin/%@.",
            self.displayName, self.cliTool];
    }
    [alert runModal];
}

@end

static MachinesAppMenu *g_menu;
static int g_menu_attempts = 0;

static NSString *shell_single_quote(NSString *text)
{
    return [text stringByReplacingOccurrencesOfString:@"'" withString:@"'\\''"];
}

void platform_host_install_app_menu(const char *display_name, const char *cli_tool)
{
    char exe[PATH_MAX];
    NSMenu *mainMenu;
    NSMenu *appMenu;
    NSInteger insertAt;
    NSMenuItem *item;

    if (!copy_executable_path(exe, sizeof(exe)) || !executable_in_bundle(exe)) {
        return;
    }
    if (NSApp == nil) {
        [NSApplication sharedApplication];
    }
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    mainMenu = NSApp.mainMenu;
    appMenu = nil;
    if (mainMenu != nil && mainMenu.numberOfItems > 0) {
        appMenu = mainMenu.itemArray.firstObject.submenu;
    }
    if (appMenu == nil) {
        /* SDL builds the menu around the first window. Try once more next turn. */
        if (g_menu_attempts < 8) {
            NSString *name = display_name != NULL ? [NSString stringWithUTF8String:display_name] : @"";
            NSString *tool = cli_tool != NULL ? [NSString stringWithUTF8String:cli_tool] : @"";
            g_menu_attempts++;
            dispatch_async(dispatch_get_main_queue(), ^{
                platform_host_install_app_menu(name.UTF8String, tool.UTF8String);
            });
        }
        return;
    }
    g_menu_attempts = 0;
    for (NSMenuItem *existing in appMenu.itemArray) {
        if (existing.action == @selector(showLog:)) {
            return;
        }
    }

    if (g_menu == nil) {
        g_menu = [MachinesAppMenu new];
    }
    g_menu.displayName = display_name != NULL ? [NSString stringWithUTF8String:display_name] : @"App";
    g_menu.cliTool = cli_tool != NULL ? [NSString stringWithUTF8String:cli_tool] : @"";

    insertAt = appMenu.numberOfItems;
    for (NSInteger i = 0; i < appMenu.numberOfItems; ++i) {
        if (appMenu.itemArray[i].action == @selector(terminate:)) {
            insertAt = i;
            break;
        }
    }
    [appMenu insertItem:[NSMenuItem separatorItem] atIndex:insertAt++];
    item = [[NSMenuItem alloc] initWithTitle:@"Show Log" action:@selector(showLog:) keyEquivalent:@""];
    item.target = g_menu;
    [appMenu insertItem:item atIndex:insertAt++];
    item = [[NSMenuItem alloc] initWithTitle:@"Show Samples" action:@selector(showSamples:) keyEquivalent:@""];
    item.target = g_menu;
    [appMenu insertItem:item atIndex:insertAt++];
    item = [[NSMenuItem alloc] initWithTitle:@"Install Command Line Tool…"
                                       action:@selector(installCli:)
                                keyEquivalent:@""];
    item.target = g_menu;
    [appMenu insertItem:item atIndex:insertAt];
}
