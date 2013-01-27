// Copyright (c) Athena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef _CHANNEL_H_
#define _CHANNEL_H_

#include "../common/mmo.h"
#include "../common/db.h"

#define MAX_USER_CHANNELS 4

enum {
	CHN_MAIN,
	CHN_VENDING,
	CHN_BATTLEGROUND,
	CHN_GAMEMASTER,
	CHN_USER,
};

struct channel_data {
	unsigned int channel_id;
	char name[NAME_LENGTH];
	char pass[NAME_LENGTH];
	// Channel Settings
	short type; // Main, Vending, Battleground, Other
	unsigned long color;
	int op, users;
	// User Database
	DBMap* users_db;
};

extern struct channel_data *server_channel[];
extern const unsigned long channel_color[];

void channel_list(struct map_session_data *sd);
bool channel_create(struct map_session_data *sd, const char* name, const char* pass, short type, short color);
void channel_join(struct map_session_data *sd, const char* name, const char* pass, bool invite);
void channel_leave(struct map_session_data *sd, const char* name, bool msg);
void channel_message(struct map_session_data *sd, const char* channel, const char* message);
int channel_invite_accept(struct map_session_data *sd);
void channel_invite_clear(struct map_session_data *sd);

void do_init_channel(void);
void do_final_channel(void);

#endif /* _CHANNEL_H_ */
