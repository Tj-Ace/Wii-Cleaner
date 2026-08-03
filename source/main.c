#include <gccore.h>
#include <malloc.h>
#include <ogc/es.h>
#include <ogc/isfs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define PATH_BUF_SIZE 96

typedef struct {
	u64 id;
	const char *code;
	const char *name;
} TargetTitle;

typedef struct {
	s32 content;
	s32 title;
	s32 tickets;
	s32 raw_title;
	s32 raw_ticket;
	bool was_found;
	bool removed;
} DeleteResult;

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

static const TargetTitle targets[] = {
	{ 0x0001000248414141ULL, "HAAA", "Photo Channel 1.0 / Photo stub" },
	{ 0x0001000248415941ULL, "HAYA", "Photo Channel 1.1" },
	{ 0x0001000248414241ULL, "HABA", "Wii Shop Channel" },
	{ 0x000100024841424BULL, "HABK", "Wii Shop Channel Korea" },
	{ 0x0001000248414641ULL, "HAFA", "Forecast Channel dummy" },
	{ 0x0001000248414645ULL, "HAFE", "Forecast Channel USA" },
	{ 0x000100024841464AULL, "HAFJ", "Forecast Channel Japan" },
	{ 0x0001000248414650ULL, "HAFP", "Forecast Channel PAL" },
	{ 0x000100024841464BULL, "HAFK", "Forecast Channel Korea" },
	{ 0x0001000248414741ULL, "HAGA", "News Channel dummy" },
	{ 0x0001000248414745ULL, "HAGE", "News Channel USA" },
	{ 0x000100024841474AULL, "HAGJ", "News Channel Japan" },
	{ 0x0001000248414750ULL, "HAGP", "News Channel PAL" },
	{ 0x000100024841474BULL, "HAGK", "News Channel Korea" },
};

static void init_video(void)
{
	VIDEO_Init();
	WPAD_Init();

	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight,
		rmode->fbWidth * VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) {
		VIDEO_WaitVSync();
	}
}

static void clear_screen(void)
{
	printf("\x1b[2J\x1b[2;0H");
}

static void wait_for_vsync(void)
{
	VIDEO_WaitVSync();
}

static u32 wait_for_buttons(void)
{
	while (SYS_MainLoop()) {
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if (pressed) {
			return pressed;
		}
		wait_for_vsync();
	}
	return 0;
}

static void make_title_dir(char *out, size_t out_size, u64 title_id)
{
	snprintf(out, out_size, "/title/%08x/%08x",
		(u32)(title_id >> 32), (u32)(title_id & 0xffffffff));
}

static void make_ticket_path(char *out, size_t out_size, u64 title_id)
{
	snprintf(out, out_size, "/ticket/%08x/%08x.tik",
		(u32)(title_id >> 32), (u32)(title_id & 0xffffffff));
}

static bool is_not_found(s32 ret)
{
	return ret == -102 || ret == -106 || ret == -101 || ret == -100;
}

static bool title_exists_es(u64 title_id)
{
	u32 size = 0;
	s32 ret = ES_GetStoredTMDSize(title_id, &size);
	return ret >= 0 && size > 0;
}

static bool path_exists_isfs(const char *path)
{
	u32 count = 0;
	s32 dir_ret = ISFS_ReadDir(path, NULL, &count);
	if (dir_ret == ISFS_OK) {
		return true;
	}

	s32 fd = ISFS_Open(path, ISFS_OPEN_READ);
	if (fd >= 0) {
		ISFS_Close(fd);
		return true;
	}

	return false;
}

static s32 delete_tree(const char *path)
{
	u32 count = 0;
	s32 ret = ISFS_ReadDir(path, NULL, &count);
	if (ret == ISFS_OK) {
		if (count > 0) {
			char *names = memalign(32, ISFS_MAXPATH * count);
			if (!names) {
				return -22;
			}

			u32 listed = count;
			ret = ISFS_ReadDir(path, names, &listed);
			if (ret != ISFS_OK) {
				free(names);
				return ret;
			}

			char *name = names;
			for (u32 i = 0; i < listed; i++) {
				char child[PATH_BUF_SIZE];
				snprintf(child, sizeof(child), "%s/%s", path, name);
				ret = delete_tree(child);
				if (ret != ISFS_OK && !is_not_found(ret)) {
					free(names);
					return ret;
				}
				name += strlen(name) + 1;
			}
			free(names);
		}
	}

	ret = ISFS_Delete(path);
	if (ret == ISFS_OK || is_not_found(ret)) {
		return ISFS_OK;
	}
	return ret;
}

static s32 delete_tickets(u64 title_id)
{
	u32 count = 0;
	s32 ret = ES_GetNumTicketViews(title_id, &count);
	if (ret != 0) {
		return ret;
	}
	if (count == 0) {
		return 0;
	}

	tikview *views = memalign(32, sizeof(tikview) * count);
	if (!views) {
		return -22;
	}

	ret = ES_GetTicketViews(title_id, views, count);
	if (ret == 0) {
		for (u32 i = 0; i < count; i++) {
			s32 ticket_ret = ES_DeleteTicket(&views[i]);
			if (ticket_ret != 0 && ret == 0) {
				ret = ticket_ret;
			}
		}
	}

	free(views);
	return ret;
}

static DeleteResult delete_target(const TargetTitle *target)
{
	DeleteResult result;
	memset(&result, 0, sizeof(result));
	result.content = 0;
	result.title = 0;
	result.tickets = 0;
	result.raw_title = 0;
	result.raw_ticket = 0;

	char title_path[PATH_BUF_SIZE];
	char ticket_path[PATH_BUF_SIZE];
	make_title_dir(title_path, sizeof(title_path), target->id);
	make_ticket_path(ticket_path, sizeof(ticket_path), target->id);

	result.was_found = title_exists_es(target->id) ||
		path_exists_isfs(title_path) ||
		path_exists_isfs(ticket_path);

	if (!result.was_found) {
		result.removed = true;
		return result;
	}

	result.content = ES_DeleteTitleContent(target->id);
	result.title = ES_DeleteTitle(target->id);
	result.tickets = delete_tickets(target->id);

	if (title_exists_es(target->id) || path_exists_isfs(title_path)) {
		result.raw_title = delete_tree(title_path);
	}
	if (path_exists_isfs(ticket_path)) {
		result.raw_ticket = ISFS_Delete(ticket_path);
		if (is_not_found(result.raw_ticket)) {
			result.raw_ticket = ISFS_OK;
		}
	}

	result.removed = !title_exists_es(target->id) &&
		!path_exists_isfs(title_path) &&
		!path_exists_isfs(ticket_path);

	return result;
}

static void print_ret(const char *label, s32 ret)
{
	if (ret == 0) {
		printf("%s OK  ", label);
	} else {
		printf("%s %d  ", label, ret);
	}
}

static void scan_titles(void)
{
	clear_screen();
	printf("Wii Cleaner - scan\n\n");
	for (u32 i = 0; i < ARRAY_SIZE(targets); i++) {
		char title_path[PATH_BUF_SIZE];
		char ticket_path[PATH_BUF_SIZE];
		make_title_dir(title_path, sizeof(title_path), targets[i].id);
		make_ticket_path(ticket_path, sizeof(ticket_path), targets[i].id);

		bool found = title_exists_es(targets[i].id) ||
			path_exists_isfs(title_path) ||
			path_exists_isfs(ticket_path);
		printf("%s  %08x-%08x  %s\n",
			found ? "[FOUND]" : "[     ]",
			(u32)(targets[i].id >> 32),
			(u32)(targets[i].id & 0xffffffff),
			targets[i].name);
	}
	printf("\nPress HOME to exit, or A to return.\n");
}

static void run_delete(void)
{
	clear_screen();
	printf("Wii Cleaner - working\n\n");
	printf("Do not power off the console.\n\n");

	for (u32 i = 0; i < ARRAY_SIZE(targets); i++) {
		printf("%s %s\n", targets[i].code, targets[i].name);
		DeleteResult result = delete_target(&targets[i]);
		if (!result.was_found) {
			printf("  not installed\n\n");
		} else {
			printf("  ");
			print_ret("content", result.content);
			print_ret("title", result.title);
			print_ret("tickets", result.tickets);
			print_ret("raw title", result.raw_title);
			print_ret("raw ticket", result.raw_ticket);
			printf("\n  final: %s\n\n", result.removed ? "removed" : "still present");
		}
		wait_for_vsync();
	}

	printf("Finished. Press HOME to exit.\n");
}

static void print_home(void)
{
	clear_screen();
	printf("Wii Cleaner\n\n");
	printf("Targets only these Wii system channels:\n");
	printf("  Photo Channel / Photo Channel 1.1\n");
	printf("  Wii Shop Channel\n");
	printf("  Forecast Channel\n");
	printf("  News Channel\n\n");
	printf("It does not target the System Menu, IOS, Mii Channel,\n");
	printf("Homebrew Channel, saves, or shared content.\n\n");
	printf("Make a NAND backup before running this.\n\n");
	printf("Controls:\n");
	printf("  A       Scan target title IDs\n");
	printf("  1 -> 2 -> PLUS   Permanently uninstall targets\n");
	printf("  HOME    Exit\n\n");
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	init_video();
	ISFS_Initialize();

	u32 confirm_step = 0;
	print_home();

	while (SYS_MainLoop()) {
		u32 pressed = wait_for_buttons();

		if (pressed & WPAD_BUTTON_HOME) {
			break;
		}

		if (pressed & WPAD_BUTTON_A) {
			scan_titles();
			while (SYS_MainLoop()) {
				u32 scan_pressed = wait_for_buttons();
				if (scan_pressed & WPAD_BUTTON_HOME) {
					ISFS_Deinitialize();
					exit(0);
				}
				if (scan_pressed & WPAD_BUTTON_A) {
					break;
				}
			}
			confirm_step = 0;
			print_home();
			continue;
		}

		if ((pressed & WPAD_BUTTON_1) && confirm_step == 0) {
			confirm_step = 1;
			printf("Confirmation step 1/3 accepted. Press 2 next.\n");
			continue;
		}
		if ((pressed & WPAD_BUTTON_2) && confirm_step == 1) {
			confirm_step = 2;
			printf("Confirmation step 2/3 accepted. Press PLUS to uninstall.\n");
			continue;
		}
		if ((pressed & WPAD_BUTTON_PLUS) && confirm_step == 2) {
			run_delete();
			while (SYS_MainLoop()) {
				u32 done_pressed = wait_for_buttons();
				if (done_pressed & WPAD_BUTTON_HOME) {
					ISFS_Deinitialize();
					exit(0);
				}
			}
		}

		if (pressed) {
			confirm_step = 0;
		}
	}

	ISFS_Deinitialize();
	return 0;
}
