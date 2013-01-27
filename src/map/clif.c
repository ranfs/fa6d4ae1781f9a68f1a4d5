// Copyright (c) Athena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include "../common/cbasetypes.h"
#include "../common/socket.h"
#include "../common/timer.h"
#include "../common/grfio.h"
#include "../common/malloc.h"
#include "../common/version.h"
#include "../common/nullpo.h"
#include "../common/harmony.h"
#include "../common/showmsg.h"
#include "../common/strlib.h"
#include "../common/utils.h"

#include "map.h"
#include "chrif.h"
#include "pc.h"
#include "status.h"
#include "npc.h"
#include "itemdb.h"
#include "channel.h"
#include "chat.h"
#include "trade.h"
#include "storage.h"
#include "script.h"
#include "skill.h"
#include "atcommand.h"
#include "intif.h"
#include "battle.h"
#include "battleground.h"
#include "mob.h"
#include "party.h"
#include "unit.h"
#include "guild.h"
#include "vending.h"
#include "harmony.h"
#include "pet.h"
#include "homunculus.h"
#include "instance.h"
#include "mercenary.h"
#include "elemental.h"
#include "log.h"
#include "clif.h"
#include "mail.h"
#include "quest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

//#define DUMP_UNKNOWN_PACKET
//#define DUMP_INVALID_PACKET

struct Clif_Config {
	int packet_db_ver;	//Preferred packet version.
	int connect_cmd[MAX_PACKET_VER + 1]; //Store the connect command for all versions. [Skotlex]
} clif_config;

struct s_packet_db packet_db[MAX_PACKET_VER + 1][MAX_PACKET_DB + 1];

//Converts item type in case of pet eggs.
inline int itemtype(int type)
{
	return ( type == IT_PETEGG ) ? IT_WEAPON : type;
}


static inline void WBUFPOS(uint8* p, unsigned short pos, short x, short y, unsigned char dir)
{
	p += pos;
	p[0] = (uint8)(x>>2);
	p[1] = (uint8)((x<<6) | ((y>>4)&0x3f));
	p[2] = (uint8)((y<<4) | (dir&0xf));
}


// client-side: x0+=sx0*0.0625-0.5 and y0+=sy0*0.0625-0.5
static inline void WBUFPOS2(uint8* p, unsigned short pos, short x0, short y0, short x1, short y1, unsigned char sx0, unsigned char sy0)
{
	p += pos;
	p[0] = (uint8)(x0>>2);
	p[1] = (uint8)((x0<<6) | ((y0>>4)&0x3f));
	p[2] = (uint8)((y0<<4) | ((x1>>6)&0x0f));
	p[3] = (uint8)((x1<<2) | ((y1>>8)&0x03));
	p[4] = (uint8)y1;
	p[5] = (uint8)((sx0<<4) | (sy0&0x0f));
}


static inline void WFIFOPOS(int fd, unsigned short pos, short x, short y, unsigned char dir)
{
	WBUFPOS(WFIFOP(fd,pos), 0, x, y, dir);
}


inline void WFIFOPOS2(int fd, unsigned short pos, short x0, short y0, short x1, short y1, unsigned char sx0, unsigned char sy0)
{
	WBUFPOS2(WFIFOP(fd,pos), 0, x0, y0, x1, y1, sx0, sy0);
}


//To idenfity disguised characters.
static inline bool disguised(struct block_list* bl)
{
	return (bool)( bl->type == BL_PC && ((TBL_PC*)bl)->disguise );
}


//Guarantees that the given string does not exceeds the allowed size, as well as making sure it's null terminated. [Skotlex]
static inline unsigned int mes_len_check(char* mes, unsigned int len, unsigned int max)
{
	if( len > max )
		len = max;

	mes[len-1] = '\0';

	return len;
}


static char map_ip_str[128];
static uint32 map_ip;
static uint32 bind_ip = INADDR_ANY;
static uint16 map_port = 5121;
int map_fd;

int clif_parse (int fd);

/*==========================================
 * @aura
 *------------------------------------------*/
static int auraTable[][3] = {
	{  -1,  -1,  -1 },
	// Reserved for PK Mode
	{ 586,  -1,  -1 }, // LH
	{ 586, 362,  -1 }, // LH Mvp
	{ 586, 362, 240 }, // 1� PK Place
	// Basic Auras
	{ 418,  -1,  -1 }, // Red Fury
	{ 486,  -1,  -1 }, // Blue Fury
	{ 485,  -1,  -1 }, // White Fury
	{ 239,  -1,  -1 }, // Aura Red
	{ 240,  -1,  -1 }, // Aura White
	{ 241,  -1,  -1 }, // Aura Yellow
	{ 620,  -1,  -1 }, // Aura Blue
	{ 202,  -1,  -1 }, // Lvl 99 Bubbles
	{ 362,  -1,  -1 }, // Advanced Lvl 99 Bubbles
	{ 678,  -1,  -1 }, // Brazil Aura Bubbles
	{ 679,  -1,  -1 }, // Brazil Aura
	{ 680,  -1,  -1 }, // Brazil Aura Floor
	// 2 Sets
	{ 239, 418,  -1 },
	{ 239, 486,  -1 },
	{ 239, 485,  -1 },
	{ 240, 418,  -1 },
	{ 240, 486,  -1 },
	{ 240, 485,  -1 },
	{ 241, 418,  -1 },
	{ 241, 486,  -1 },
	{ 241, 485,  -1 },
	{ 620, 418,  -1 },
	{ 620, 486,  -1 },
	{ 620, 485,  -1 },
	// Full Sets
	{ 239, 418, 202 },
	{ 239, 486, 202 },
	{ 239, 485, 202 },
	{ 240, 418, 202 },
	{ 240, 486, 202 },
	{ 240, 485, 202 },
	{ 241, 418, 202 },
	{ 241, 486, 202 },
	{ 241, 485, 202 },
	{ 620, 418, 202 },
	{ 620, 486, 202 },
	{ 620, 485, 202 },
	{ 239, 418, 362 },
	{ 239, 486, 362 },
	{ 239, 485, 362 },
	{ 240, 418, 362 },
	{ 240, 486, 362 },
	{ 240, 485, 362 },
	{ 241, 418, 362 },
	{ 241, 486, 362 },
	{ 241, 485, 362 },
	{ 620, 418, 362 },
	{ 620, 486, 362 },
	{ 620, 485, 362 },
	{ 239, 418, 678 },
	{ 239, 486, 678 },
	{ 239, 485, 678 },
	{ 240, 418, 678 },
	{ 240, 486, 678 },
	{ 240, 485, 678 },
	{ 241, 418, 678 },
	{ 241, 486, 678 },
	{ 241, 485, 678 },
	{ 620, 418, 678 },
	{ 620, 486, 678 },
	{ 620, 485, 678 },
	// Oficial Set
	{ 680, 679, 678 },
	{  -1,  -1,  -1 }
};

int aura_getSize()
{
	return sizeof(auraTable)/(sizeof(int) * 3) - 1;
}

int aura_getAuraEffect(struct map_session_data *sd, short pos)
{
	int aura = sd->view_aura;
	if( pos < 0 || pos > 2 )
		return -1;
	if( aura > aura_getSize() || aura < 0 )
		return -1;

	return auraTable[aura][pos];
}

void clif_sendaurastoone(struct map_session_data *sd, struct map_session_data *dsd)
{
	int effect1, effect2, effect3;

	if( pc_ishiding(sd) || map_flag_vs(sd->bl.m) )
		return;

	effect1 = aura_getAuraEffect(sd,0);
	effect2 = aura_getAuraEffect(sd,1);
	effect3 = aura_getAuraEffect(sd,2);

	if( effect1 >= 0 )
		clif_specialeffect_single(&sd->bl,effect1,dsd->fd);
	if( effect2 >= 0 )
		clif_specialeffect_single(&sd->bl,effect2,dsd->fd);
	if( effect3 >= 0 )
		clif_specialeffect_single(&sd->bl,effect3,dsd->fd);
}

void clif_sendauras(struct map_session_data *sd,  enum send_target type)
{
	int effect1, effect2, effect3;

	if( pc_ishiding(sd) || map_flag_vs(sd->bl.m) )
		return;

	effect1 = aura_getAuraEffect(sd,0);
	effect2 = aura_getAuraEffect(sd,1);
	effect3 = aura_getAuraEffect(sd,2);

	if( effect1 >= 0 )
		clif_specialeffect(&sd->bl,effect1,type);
	if( effect2 >= 0 )
		clif_specialeffect(&sd->bl,effect2,type);
	if( effect3 >= 0 )
		clif_specialeffect(&sd->bl,effect3,type);
}

/*==========================================
 * map�I��ip�ݒ�
 *------------------------------------------*/
int clif_setip(const char* ip)
{
	char ip_str[16];
	map_ip = host2ip(ip);
	if (!map_ip) {
		ShowWarning("Failed to Resolve Map Server Address! (%s)\n", ip);
		return 0;
	}

	strncpy(map_ip_str, ip, sizeof(map_ip_str));
	ShowInfo("Map Server IP Address : '"CL_WHITE"%s"CL_RESET"' -> '"CL_WHITE"%s"CL_RESET"'.\n", ip, ip2str(map_ip, ip_str));
	return 1;
}

void clif_setbindip(const char* ip)
{
	char ip_str[16];
	bind_ip = host2ip(ip);
	if (bind_ip) {
		ShowInfo("Map Server Bind IP Address : '"CL_WHITE"%s"CL_RESET"' -> '"CL_WHITE"%s"CL_RESET"'.\n", ip, ip2str(bind_ip, ip_str));
	} else {
		ShowWarning("Failed to Resolve Map Server Address! (%s)\n", ip);
	}
}

/*==========================================
 * map�I��port�ݒ�
 *------------------------------------------*/
void clif_setport(uint16 port)
{
	map_port = port;
}

/*==========================================
 * map�I��ip�ǂݏo��
 *------------------------------------------*/
uint32 clif_getip(void)
{
	return map_ip;
}

//Refreshes map_server ip, returns the new ip if the ip changed, otherwise it returns 0.
uint32 clif_refresh_ip(void)
{
	uint32 new_ip;

	new_ip = host2ip(map_ip_str);
	if (new_ip && new_ip != map_ip) {
		map_ip = new_ip;
		ShowInfo("Updating IP resolution of [%s].\n", map_ip_str);
		return map_ip;
	}
	return 0;
}

/*==========================================
 * map�I��port�ǂݏo��
 *------------------------------------------*/
uint16 clif_getport(void)
{
	return map_port;
}

#if PACKETVER >= 20071106
static inline unsigned char clif_bl_type(struct block_list *bl) {
	switch (bl->type) {
	case BL_PC:    return disguised(bl)?0x1:0x0; //PC_TYPE
	case BL_ITEM:  return 0x2; //ITEM_TYPE
	case BL_SKILL: return 0x3; //SKILL_TYPE
	case BL_CHAT:  return 0x4; //UNKNOWN_TYPE
	case BL_MOB:   return pcdb_checkid(status_get_viewdata(bl)->class_)?0x0:0x5; //NPC_MOB_TYPE
	case BL_NPC:   return 0x6; //NPC_EVT_TYPE
	case BL_PET:   return pcdb_checkid(status_get_viewdata(bl)->class_)?0x0:0x7; //NPC_PET_TYPE
	case BL_HOM:   return 0x8; //NPC_HOM_TYPE
	case BL_MER:   return 0x9; //NPC_MERSOL_TYPE
	case BL_ELEM:  return 0xa; //NPC_ELEMENTAL_TYPE
	default:       return 0x1; //NPC_TYPE
	}
}
#endif

/*==========================================
 * clif_send��AREA*�w�莞�p
 *------------------------------------------*/
int clif_send_sub(struct block_list *bl, va_list ap)
{
	struct block_list *src_bl;
	struct map_session_data *sd;
	unsigned char *buf;
	int len, type, fd;

	nullpo_ret(bl);
	nullpo_ret(sd = (struct map_session_data *)bl);

	fd = sd->fd;
	if (!fd) //Don't send to disconnected clients.
		return 0;

	buf = va_arg(ap,unsigned char*);
	len = va_arg(ap,int);
	nullpo_ret(src_bl = va_arg(ap,struct block_list*));
	type = va_arg(ap,int);

	switch(type)
	{
	case AREA:
		if( RBUFW(buf,0) == 0x01c8 && (map[sd->bl.m].flag.gvg || map[sd->bl.m].flag.battleground) && bl != src_bl && sd->state.packet_filter&2 )
			return 0; // Ignore other player's item usage
	break;
	case AREA_WOS:
		if (bl == src_bl)
			return 0;
	break;
	case AREA_WOC:
		if (sd->chatID || bl == src_bl)
			return 0;
		if( RBUFW(buf,0) == 0x008d && (map[sd->bl.m].flag.gvg || map[sd->bl.m].flag.battleground) && sd->state.packet_filter&1 )
			return 0;
	break;
	case AREA_WOSC:
	{
		struct map_session_data *ssd = (struct map_session_data *)src_bl;
		if (ssd && (src_bl->type == BL_PC) && sd->chatID && (sd->chatID == ssd->chatID))
			return 0;
	}
	break;
	case AREA_IWOS:
		if( bl == src_bl )
			return 0;
	case AREA_IWS:
		if( bl != src_bl && !sd->special_state.intravision && !sd->sc.data[SC_INTRAVISION] )
			return 0;
	break;
	case AREA_WOI:
		if( bl == src_bl || sd->special_state.intravision || sd->sc.data[SC_INTRAVISION] )
			return 0;
	break;
	}

	if (session[fd] == NULL)
		return 0;

	WFIFOHEAD(fd, len);
	if (WFIFOP(fd,0) == buf) {
		ShowError("WARNING: Invalid use of clif_send function\n");
		ShowError("         Packet x%4x use a WFIFO of a player instead of to use a buffer.\n", WBUFW(buf,0));
		ShowError("         Please correct your code.\n");
		// don't send to not move the pointer of the packet for next sessions in the loop
		//WFIFOSET(fd,0);//## TODO is this ok?
		//NO. It is not ok. There is the chance WFIFOSET actually sends the buffer data, and shifts elements around, which will corrupt the buffer.
		return 0;
	}

	if (packet_db[sd->packet_ver][RBUFW(buf,0)].len) { // packet must exist for the client version
		memcpy(WFIFOP(fd,0), buf, len);
		WFIFOSET(fd,len);
	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_send(const uint8* buf, int len, struct block_list* bl, enum send_target type)
{
	int i;
	struct map_session_data *sd, *tsd;
	struct party_data *p = NULL;
	struct guild *g = NULL;
	struct battleground_data *bg = NULL;
	int x0 = 0, x1 = 0, y0 = 0, y1 = 0, fd;
	struct s_mapiterator* iter;

	if( type != ALL_CLIENT )
		nullpo_ret(bl);

	if( type == ALL_REGION && map[bl->m].region < 1 )
		return 0; // Not on a Region

	sd = BL_CAST(BL_PC, bl);

	switch(type) {

	case ALL_CLIENT: //All player clients.
		iter = mapit_getallusers();
		while( (tsd = (TBL_PC*)mapit_next(iter)) != NULL )
		{
			if( packet_db[tsd->packet_ver][RBUFW(buf,0)].len )
			{ // packet must exist for the client version
				WFIFOHEAD(tsd->fd, len);
				memcpy(WFIFOP(tsd->fd,0), buf, len);
				WFIFOSET(tsd->fd,len);
			}
		}
		mapit_free(iter);
		break;

	case ALL_SAMEMAP: //All players on the same map
	case ALL_REGION: // All players on the same region
		iter = mapit_getallusers();
		while( (tsd = (TBL_PC*)mapit_next(iter)) != NULL )
		{
			if( ((type == ALL_SAMEMAP && tsd->bl.m == bl->m) || (type == ALL_REGION && map[tsd->bl.m].region == map[bl->m].region)) && packet_db[tsd->packet_ver][RBUFW(buf,0)].len )
			{ // packet must exist for the client version
				WFIFOHEAD(tsd->fd, len);
				memcpy(WFIFOP(tsd->fd,0), buf, len);
				WFIFOSET(tsd->fd,len);
			}
		}
		mapit_free(iter);
		break;

	case AREA:
	case AREA_WOSC:
		if (sd && bl->prev == NULL) //Otherwise source misses the packet.[Skotlex]
			clif_send (buf, len, bl, SELF);
	case AREA_WOC:
	case AREA_WOS:
		map_foreachinarea(clif_send_sub, bl->m, bl->x-AREA_SIZE, bl->y-AREA_SIZE, bl->x+AREA_SIZE, bl->y+AREA_SIZE,
			BL_PC, buf, len, bl, type);
		break;
	case AREA_CHAT_WOC:
		map_foreachinarea(clif_send_sub, bl->m, bl->x-(AREA_SIZE-5), bl->y-(AREA_SIZE-5),
			bl->x+(AREA_SIZE-5), bl->y+(AREA_SIZE-5), BL_PC, buf, len, bl, AREA_WOC);
		break;
	case AREA_IWS:
	case AREA_IWOS:
	case AREA_WOI:
		map_foreachinarea(clif_send_sub, bl->m, bl->x-AREA_SIZE, bl->y-AREA_SIZE, bl->x+AREA_SIZE, bl->y+AREA_SIZE,
			BL_PC, buf, len, bl, type);
		break;

	case CHAT:
	case CHAT_WOS:
		{
			struct chat_data *cd;
			if (sd) {
				cd = (struct chat_data*)map_id2bl(sd->chatID);
			} else if (bl->type == BL_CHAT) {
				cd = (struct chat_data*)bl;
			} else break;
			if (cd == NULL)
				break;
			for(i = 0; i < cd->users; i++) {
				if (type == CHAT_WOS && cd->usersd[i] == sd)
					continue;
				if (packet_db[cd->usersd[i]->packet_ver][RBUFW(buf,0)].len) { // packet must exist for the client version
					if ((fd=cd->usersd[i]->fd) >0 && session[fd]) // Added check to see if session exists [PoW]
					{
						WFIFOHEAD(fd,len);
						memcpy(WFIFOP(fd,0), buf, len);
						WFIFOSET(fd,len);
					}
				}
			}
		}
		break;
	case PARTY_AREA:
	case PARTY_AREA_WOS:
		x0 = bl->x - AREA_SIZE;
		y0 = bl->y - AREA_SIZE;
		x1 = bl->x + AREA_SIZE;
		y1 = bl->y + AREA_SIZE;
	case PARTY:
	case PARTY_WOS:
	case PARTY_SAMEMAP:
	case PARTY_SAMEMAP_WOS:
		if( sd && sd->status.party_id )
			p = party_search(sd->status.party_id);
			
		if( p )
		{
			for( i = 0; i < MAX_PARTY; i++ )
			{
				if( (sd = p->data[i].sd) == NULL )
					continue;

				if( !(fd = sd->fd) )
					continue;

				if( sd->bl.id == bl->id && (type == PARTY_WOS || type == PARTY_SAMEMAP_WOS || type == PARTY_AREA_WOS) )
					continue;
				
				if( type != PARTY && type != PARTY_WOS && bl->m != sd->bl.m )
					continue;
				
				if( (type == PARTY_AREA || type == PARTY_AREA_WOS) && (sd->bl.x < x0 || sd->bl.y < y0 || sd->bl.x > x1 || sd->bl.y > y1) )
					continue;
				
				if( packet_db[sd->packet_ver][RBUFW(buf,0)].len )
				{ // packet must exist for the client version
					WFIFOHEAD(fd,len);
					memcpy(WFIFOP(fd,0), buf, len);
					WFIFOSET(fd,len);
				}
			}
			if (!enable_spy) //Skip unnecessary parsing. [Skotlex]
				break;

			iter = mapit_getallusers();
			while( (tsd = (TBL_PC*)mapit_next(iter)) != NULL )
			{
				if( tsd->partyspy == p->party.party_id && packet_db[tsd->packet_ver][RBUFW(buf,0)].len )
				{ // packet must exist for the client version
					WFIFOHEAD(tsd->fd, len);
					memcpy(WFIFOP(tsd->fd,0), buf, len);
					WFIFOSET(tsd->fd,len);
				}
			}
			mapit_free(iter);
		}
		break;

	case DUEL:
	case DUEL_WOS:
		if (!sd || !sd->duel_group) break; //Invalid usage.

		iter = mapit_getallusers();
		while( (tsd = (TBL_PC*)mapit_next(iter)) != NULL )
		{
			if( type == DUEL_WOS && bl->id == tsd->bl.id )
				continue;
			if( sd->duel_group == tsd->duel_group && packet_db[tsd->packet_ver][RBUFW(buf,0)].len )
			{ // packet must exist for the client version
				WFIFOHEAD(tsd->fd, len);
				memcpy(WFIFOP(tsd->fd,0), buf, len);
				WFIFOSET(tsd->fd,len);
			}
		}
		mapit_free(iter);
		break;

	case SELF:
		if (sd && (fd=sd->fd) && packet_db[sd->packet_ver][RBUFW(buf,0)].len) { // packet must exist for the client version
			WFIFOHEAD(fd,len);
			memcpy(WFIFOP(fd,0), buf, len);
			WFIFOSET(fd,len);
		}
		break;

	// New definitions for guilds [Valaris] - Cleaned up and reorganized by [Skotlex]
	case GUILD_AREA:
	case GUILD_AREA_WOS:
		x0 = bl->x - AREA_SIZE;
		y0 = bl->y - AREA_SIZE;
		x1 = bl->x + AREA_SIZE;
		y1 = bl->y + AREA_SIZE;
	case GUILD_SAMEMAP:
	case GUILD_SAMEMAP_WOS:
	case GUILD:
	case GUILD_WOS:
	case GUILD_NOBG:
		if (sd && sd->status.guild_id)
			g = guild_search(sd->status.guild_id);

		if (g) {
			for(i = 0; i < g->max_member; i++) {
				if( (sd = g->member[i].sd) != NULL )
				{
					if( !(fd=sd->fd) )
						continue;
					
					if( type == GUILD_NOBG && sd->bg_id )
						continue;

					if( sd->bl.id == bl->id && (type == GUILD_WOS || type == GUILD_SAMEMAP_WOS || type == GUILD_AREA_WOS) )
						continue;
					
					if( type != GUILD && type != GUILD_NOBG && type != GUILD_WOS && sd->bl.m != bl->m )
						continue;

					if( (type == GUILD_AREA || type == GUILD_AREA_WOS) && (sd->bl.x < x0 || sd->bl.y < y0 || sd->bl.x > x1 || sd->bl.y > y1) )
						continue;

					if( packet_db[sd->packet_ver][RBUFW(buf,0)].len )
					{ // packet must exist for the client version
						WFIFOHEAD(fd,len);
						memcpy(WFIFOP(fd,0), buf, len);
						WFIFOSET(fd,len);
					}
				}
			}
			if (!enable_spy) //Skip unnecessary parsing. [Skotlex]
				break;

			iter = mapit_getallusers();
			while( (tsd = (TBL_PC*)mapit_next(iter)) != NULL )
			{
				if( tsd->guildspy == g->guild_id && packet_db[tsd->packet_ver][RBUFW(buf,0)].len )
				{ // packet must exist for the client version
					WFIFOHEAD(tsd->fd, len);
					memcpy(WFIFOP(tsd->fd,0), buf, len);
					WFIFOSET(tsd->fd,len);
				}
			}
			mapit_free(iter);
		}
		break;

	case BG_AREA:
	case BG_AREA_WOS:
		x0 = bl->x - AREA_SIZE;
		y0 = bl->y - AREA_SIZE;
		x1 = bl->x + AREA_SIZE;
		y1 = bl->y + AREA_SIZE;
	case BG_SAMEMAP:
	case BG_SAMEMAP_WOS:
	case BG:
	case BG_WOS:
		if( sd && sd->bg_id && (bg = bg_team_search(sd->bg_id)) != NULL )
		{
			for( i = 0; i < MAX_BG_MEMBERS; i++ )
			{
				if( (sd = bg->members[i].sd) == NULL || !(fd = sd->fd) )
					continue;
				if( sd->bl.id == bl->id && (type == BG_WOS || type == BG_SAMEMAP_WOS || type == BG_AREA_WOS) )
					continue;
				if( type != BG && type != BG_WOS && sd->bl.m != bl->m )
					continue;
				if( (type == BG_AREA || type == BG_AREA_WOS) && (sd->bl.x < x0 || sd->bl.y < y0 || sd->bl.x > x1 || sd->bl.y > y1) )
					continue;
				if( packet_db[sd->packet_ver][RBUFW(buf,0)].len )
				{ // packet must exist for the client version
					WFIFOHEAD(fd,len);
					memcpy(WFIFOP(fd,0), buf, len);
					WFIFOSET(fd,len);
				}
			}
		}
		break;

	default:
		ShowError("clif_send: Unrecognized type %d\n",type);
		return -1;
	}

	return 0;
}

//
// �p�P�b�g����đ��M
//
/*==========================================
 *
 *------------------------------------------*/
int clif_authok(struct map_session_data *sd)
{
	int fd;

	if (!sd->fd)
		return 0;
	fd = sd->fd;

	WFIFOHEAD(fd, packet_len(0x73));
	WFIFOW(fd, 0) = 0x73;
	WFIFOL(fd, 2) = gettick();
	WFIFOPOS(fd, 6, sd->bl.x, sd->bl.y, sd->ud.dir);
	WFIFOB(fd, 9) = 5; // ignored
	WFIFOB(fd,10) = 5; // ignored
	WFIFOSET(fd,packet_len(0x73));

	return 0;
}

/*==========================================
 * Authentication failed/disconnect client.
 *------------------------------------------
 * The client closes it's socket and displays a message according to type:
 *  1 - server closed -> MsgStringTable[4]
 *  2 - ID already logged in -> MsgStringTable[5]
 *  3 - timeout/too much lag -> MsgStringTable[241]
 *  4 - server full -> MsgStringTable[264]
 *  5 - underaged -> MsgStringTable[305]
 *  8 - Server sill recognizes last connection -> MsgStringTable[441]
 *  9 - too many connections from this ip -> MsgStringTable[529]
 *  10 - out of available time paid for -> MsgStringTable[530]
 *  15 - disconnected by a GM -> if( servicetype == taiwan ) MsgStringTable[579]
 *  other - disconnected -> MsgStringTable[3]
 */
int clif_authfail_fd(int fd, int type)
{
	if (!fd || !session[fd] || session[fd]->func_parse != clif_parse) //clif_authfail should only be invoked on players!
		return 0;

	WFIFOHEAD(fd, packet_len(0x81));
	WFIFOW(fd,0) = 0x81;
	WFIFOB(fd,2) = type;
	WFIFOSET(fd,packet_len(0x81));
	set_eof(fd);
	return 0;
}

/// Reply from char-server.
/// Tells the player if it can connect to the char-server to select a character.
/// ok=1 : client disconnects and tries to connect to the char-server
int clif_charselectok(int id, uint8 ok)
{
	struct map_session_data* sd;
	int fd;

	if ((sd = map_id2sd(id)) == NULL || !sd->fd)
		return 1;

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0xb3));
	WFIFOW(fd,0) = 0xb3;
	WFIFOB(fd,2) = ok;
	WFIFOSET(fd,packet_len(0xb3));

	return 0;
}

/*==========================================
 * Makes an item appear on the ground
 * 009e <ID>.l <name ID>.w <identify flag>.B <X>.w <Y>.w <subX>.B <subY>.B <amount>.w
 *------------------------------------------*/
int clif_dropflooritem(struct flooritem_data* fitem)
{
	uint8 buf[17];
	int view;

	nullpo_ret(fitem);

	if (fitem->item_data.nameid <= 0)
		return 0;

	WBUFW(buf, 0) = 0x9e;
	WBUFL(buf, 2) = fitem->bl.id;
	WBUFW(buf, 6) = ((view = itemdb_viewid(fitem->item_data.nameid)) > 0) ? view : fitem->item_data.nameid;
	WBUFB(buf, 8) = fitem->item_data.identify;
	WBUFW(buf, 9) = fitem->bl.x;
	WBUFW(buf,11) = fitem->bl.y;
	WBUFB(buf,13) = fitem->subx;
	WBUFB(buf,14) = fitem->suby;
	WBUFW(buf,15) = fitem->item_data.amount;

	clif_send(buf, packet_len(0x9e), &fitem->bl, AREA);

	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_clearflooritem(struct flooritem_data *fitem, int fd)
{
	unsigned char buf[16];

	nullpo_ret(fitem);

	WBUFW(buf,0) = 0xa1;
	WBUFL(buf,2) = fitem->bl.id;

	if (fd == 0) {
		clif_send(buf, packet_len(0xa1), &fitem->bl, AREA);
	} else {
		WFIFOHEAD(fd,packet_len(0xa1));
		memcpy(WFIFOP(fd,0), buf, packet_len(0xa1));
		WFIFOSET(fd,packet_len(0xa1));
	}

	return 0;
}

/*==========================================
 * make a unit (char, npc, mob, homun) disappear to one client
 * id  : the id of the unit
 * type: 0 - moved out of sight
 *       1 - died
 *       2 - respawned
 *       3 - teleported / logged out
 * fd  : the target client
 *------------------------------------------*/
int clif_clearunit_single(int id, clr_type type, int fd)
{
	WFIFOHEAD(fd, packet_len(0x80));
	WFIFOW(fd,0) = 0x80;
	WFIFOL(fd,2) = id;
	WFIFOB(fd,6) = type;
	WFIFOSET(fd, packet_len(0x80));

	return 0;
}

/*==========================================
 * make a unit (char, npc, mob, homun) disappear to all clients in area
 * type: 0 - moved out of sight
 *       1 - died
 *       2 - respawned
 *       3 - teleported / logged out
 *------------------------------------------*/
int clif_clearunit_area(struct block_list* bl, clr_type type)
{
	unsigned char buf[16];

	nullpo_ret(bl);

	WBUFW(buf,0) = 0x80;
	WBUFL(buf,2) = bl->id;
	WBUFB(buf,6) = type;

	clif_send(buf, packet_len(0x80), bl, type == CLR_DEAD ? AREA : AREA_WOS);

	if(disguised(bl)) {
		WBUFL(buf,2) = -bl->id;
		clif_send(buf, packet_len(0x80), bl, SELF);
	}

	return 0;
}

int clif_clearunit_invisible(struct block_list *bl)
{
	unsigned char buf[16];
	nullpo_ret(bl);
	WBUFW(buf,0) = 0x80;
	WBUFL(buf,2) = bl->id;
	WBUFB(buf,6) = CLR_OUTSIGHT;
	clif_send(buf, packet_len(0x80), bl, AREA_WOI);

	return 0;
}

static int clif_clearunit_delayed_sub(int tid, unsigned int tick, int id, intptr_t data)
{
	struct block_list *bl = (struct block_list *)data;
	clif_clearunit_area(bl, CLR_OUTSIGHT);
	aFree(bl);
	return 0;
}

int clif_clearunit_delayed(struct block_list* bl, unsigned int tick)
{
	struct block_list *tbl;
	tbl = (struct block_list*)aMalloc(sizeof (struct block_list));
	memcpy (tbl, bl, sizeof (struct block_list));
	add_timer(tick, clif_clearunit_delayed_sub, 0, (intptr_t)tbl);
	return 0;
}

void clif_get_weapon_view(struct map_session_data* sd, unsigned short *rhand, unsigned short *lhand)
{
	if(sd->sc.option&(OPTION_WEDDING|OPTION_XMAS|OPTION_SUMMER))
	{
		*rhand = *lhand = 0;
		return;
	}

#if PACKETVER < 4
	*rhand = sd->status.weapon;
	*lhand = sd->status.shield;
#else
	if (sd->equip_index[EQI_HAND_R] >= 0 &&
		sd->inventory_data[sd->equip_index[EQI_HAND_R]]) 
	{
		struct item_data* id = sd->inventory_data[sd->equip_index[EQI_HAND_R]];
		if (id->view_id > 0)
			*rhand = id->view_id;
		else
			*rhand = id->nameid;
	} else
		*rhand = 0;

	if (sd->equip_index[EQI_HAND_L] >= 0 &&
		sd->equip_index[EQI_HAND_L] != sd->equip_index[EQI_HAND_R] &&
		sd->inventory_data[sd->equip_index[EQI_HAND_L]]) 
	{
		struct item_data* id = sd->inventory_data[sd->equip_index[EQI_HAND_L]];
		if (id->view_id > 0)
			*lhand = id->view_id;
		else
			*lhand = id->nameid;
	} else
		*lhand = 0;
#endif
}

//To make the assignation of the level based on limits clearer/easier. [Skotlex]
static int clif_setlevel_sub(int lv)
{
	if( lv < battle_config.max_lv )
	{
		;
	}
	else if( lv < battle_config.aura_lv )
	{
		lv = battle_config.max_lv - 1;
	}
	else
	{
		lv = battle_config.max_lv;
	}

	return lv;
}

static int clif_setlevel(struct block_list* bl)
{
	int lv = status_get_lv(bl);
	if( battle_config.client_limit_unit_lv&bl->type )
		return clif_setlevel_sub(lv);
	switch( bl->type )
	{
		case BL_NPC:
		case BL_PET:
			// npcs and pets do not have level
			return 0;
	}
	return lv;
}

/*==========================================
 * Prepares 'unit standing/spawning' packet
 *------------------------------------------*/
static int clif_set_unit_idle(struct block_list* bl, unsigned char* buffer, bool spawn)
{
	struct map_session_data* sd;
	struct status_change* sc = status_get_sc(bl);
	struct view_data* vd = status_get_viewdata(bl);
	unsigned char *buf = WBUFP(buffer,0);
#if PACKETVER < 20091103
	bool type = !pcdb_checkid(vd->class_);
#endif
#if PACKETVER >= 7
	unsigned short offset = 0;
#endif
#if PACKETVER >= 20091103
	const char *name;
#endif
	sd = BL_CAST(BL_PC, bl);

#if PACKETVER < 20091103
	if(type)
		WBUFW(buf,0) = spawn?0x7c:0x78;
	else
#endif
#if PACKETVER < 4
		WBUFW(buf,0) = spawn?0x79:0x78;
#elif PACKETVER < 7
		WBUFW(buf,0) = spawn?0x1d9:0x1d8;
#elif PACKETVER < 20080102
		WBUFW(buf,0) = spawn?0x22b:0x22a;
#elif PACKETVER < 20091103
		WBUFW(buf,0) = spawn?0x2ed:0x2ee;
#elif PACKETVER < 20101124
		WBUFW(buf,0) = spawn?0x7f8:0x7f9;
#else
		WBUFW(buf,0) = spawn?0x858:0x857;
#endif

#if PACKETVER >= 20091103
	name = status_get_name(bl);
#if PACKETVER < 20110111
	WBUFW(buf,2) = (spawn?62:63)+strlen(name);
#else
	WBUFW(buf,2) = (spawn?64:65)+strlen(name);
#endif
	WBUFB(buf,4) = clif_bl_type(bl);
	offset+=3;
	buf = WBUFP(buffer,offset);
#elif PACKETVER >= 20071106
	if (type) { //Non-player packets
		WBUFB(buf,2) = clif_bl_type(bl);
		offset++;
		buf = WBUFP(buffer,offset);
	}
#endif
	WBUFL(buf, 2) = bl->id;
	WBUFW(buf, 6) = status_get_speed(bl);
	WBUFW(buf, 8) = (sc)? sc->opt1 : 0;
	WBUFW(buf,10) = (sc)? sc->opt2 : 0;
#if PACKETVER < 20091103
	if (type&&spawn) { //uses an older and different packet structure
		WBUFW(buf,12) = (sc)? sc->option : 0;
		WBUFW(buf,14) = vd->hair_style;
		WBUFW(buf,16) = vd->weapon;
		WBUFW(buf,18) = vd->head_bottom;
		WBUFW(buf,20) = vd->class_; //Pet armor (ignored by client)
		WBUFW(buf,22) = vd->shield;
	} else {
#endif
#if PACKETVER >= 20091103
		WBUFL(buf,12) = (sc)? sc->option : 0;
		offset+=2;
		buf = WBUFP(buffer,offset);
#elif PACKETVER >= 7
		if (!type) {
			WBUFL(buf,12) = (sc)? sc->option : 0;
			offset+=2;
			buf = WBUFP(buffer,offset);
		} else
			WBUFW(buf,12) = (sc)? sc->option : 0;
#else
		WBUFW(buf,12) = (sc)? sc->option : 0;
#endif
		WBUFW(buf,14) = vd->class_;
		WBUFW(buf,16) = vd->hair_style;
		WBUFW(buf,18) = vd->weapon;
#if PACKETVER < 4
		WBUFW(buf,20) = vd->head_bottom;
		WBUFW(buf,22) = vd->shield;
#else
		WBUFW(buf,20) = vd->shield;
		WBUFW(buf,22) = vd->head_bottom;
#endif
#if PACKETVER < 20091103
	}
#endif
	WBUFW(buf,24) = vd->head_top;
	WBUFW(buf,26) = vd->head_mid;

	if( bl->type == BL_NPC && vd->class_ == FLAG_CLASS )
	{	//The hell, why flags work like this?
		WBUFL(buf,22) = clif_visual_emblem_id(bl);
		WBUFL(buf,26) = clif_visual_guild_id(bl);
	}

	WBUFW(buf,28) = vd->hair_color;
	WBUFW(buf,30) = vd->cloth_color;
	WBUFW(buf,32) = (sd)? sd->head_dir : 0;
#if PACKETVER < 20091103
	if (type&&spawn) { //End of packet 0x7c
		WBUFB(buf,34) = (sd)?sd->status.karma:0; // karma
		WBUFB(buf,35) = vd->sex;
		WBUFPOS(buf,36,bl->x,bl->y,unit_getdir(bl));
		WBUFB(buf,39) = 0;
		WBUFB(buf,40) = 0;
		return packet_len(0x7c);
	}
#endif
#if PACKETVER >= 20110111
	WBUFW(buf,34) = vd->robe;
	offset+= 2;
	buf = WBUFP(buffer,offset);
#endif
	WBUFL(buf,34) = clif_visual_guild_id(bl);
	WBUFW(buf,38) = clif_visual_emblem_id(bl);
	WBUFW(buf,40) = (sd)? sd->status.manner : 0;
#if PACKETVER >= 20091103
	WBUFL(buf,42) = (sc)? sc->opt3 : 0;
	offset+=2;
	buf = WBUFP(buffer,offset);
#elif PACKETVER >= 7
	if (!type) {
		WBUFL(buf,42) = (sc)? sc->opt3 : 0;
		offset+=2;
		buf = WBUFP(buffer,offset);
	} else
		WBUFW(buf,42) = (sc)? sc->opt3 : 0;
#else
	WBUFW(buf,42) = (sc)? sc->opt3 : 0;
#endif
	WBUFB(buf,44) = (sd)? sd->status.karma : 0;
	WBUFB(buf,45) = vd->sex;
	WBUFPOS(buf,46,bl->x,bl->y,unit_getdir(bl));
	WBUFB(buf,49) = (sd)? 5 : 0;
	WBUFB(buf,50) = (sd)? 5 : 0;
	if (!spawn) {
		WBUFB(buf,51) = vd->dead_sit;
		offset++;
		buf = WBUFP(buffer,offset);
	}
	WBUFW(buf,51) = clif_setlevel(bl);
#if PACKETVER < 20091103
	if (type) //End for non-player packet
		return packet_len(WBUFW(buffer,0));
#endif
#if PACKETVER >= 20080102
	WBUFW(buf,53) = sd?sd->user_font:0;
#endif
#if PACKETVER >= 20091103
	strcpy((char*)WBUFP(buf,55), name);
	return WBUFW(buffer,2);
#else
	return packet_len(WBUFW(buffer,0));
#endif
}

/*==========================================
 * Prepares 'unit walking' packet
 *------------------------------------------*/
static int clif_set_unit_walking(struct block_list* bl, struct unit_data* ud, unsigned char* buffer)
{
	struct map_session_data* sd;
	struct status_change* sc = status_get_sc(bl);
	struct view_data* vd = status_get_viewdata(bl);
	unsigned char* buf = WBUFP(buffer,0);
#if PACKETVER >= 7
	unsigned short offset = 0;
#endif
#if PACKETVER >= 20091103
	const char *name;
#endif

	sd = BL_CAST(BL_PC, bl);

#if PACKETVER < 4
	WBUFW(buf, 0) = 0x7b;
#elif PACKETVER < 7
	WBUFW(buf, 0) = 0x1da;
#elif PACKETVER < 20080102
	WBUFW(buf, 0) = 0x22c;
#elif PACKETVER < 20091103
	WBUFW(buf, 0) = 0x2ec;
#elif PACKETVER < 20101124
	WBUFW(buf, 0) = 0x7f7;
#else
	WBUFW(buf, 0) = 0x856;
#endif

#if PACKETVER >= 20091103
	name = status_get_name(bl);
#if PACKETVER < 20110111
	WBUFW(buf, 2) = 69+strlen(name);
#else
	WBUFW(buf, 2) = 71+strlen(name);
#endif
	offset+=2;
	buf = WBUFP(buffer,offset);
#endif
#if PACKETVER >= 20071106
	WBUFB(buf, 2) = clif_bl_type(bl);
	offset++;
	buf = WBUFP(buffer,offset);
#endif
	WBUFL(buf, 2) = bl->id;
	WBUFW(buf, 6) = status_get_speed(bl);
	WBUFW(buf, 8) = (sc)? sc->opt1 : 0;
	WBUFW(buf,10) = (sc)? sc->opt2 : 0;
#if PACKETVER < 7
	WBUFW(buf,12) = (sc)? sc->option : 0;
#else
	WBUFL(buf,12) = (sc)? sc->option : 0;
	offset+=2; //Shift the rest of elements by 2 bytes.
	buf = WBUFP(buffer,offset);
#endif
	WBUFW(buf,14) = vd->class_;
	WBUFW(buf,16) = vd->hair_style;
	WBUFW(buf,18) = vd->weapon;
#if PACKETVER < 4
	WBUFW(buf,20) = vd->head_bottom;
	WBUFL(buf,22) = gettick();
	WBUFW(buf,26) = vd->shield;
#else
	WBUFW(buf,20) = vd->shield;
	WBUFW(buf,22) = vd->head_bottom;
	WBUFL(buf,24) = gettick();
#endif
	WBUFW(buf,28) = vd->head_top;
	WBUFW(buf,30) = vd->head_mid;
	WBUFW(buf,32) = vd->hair_color;
	WBUFW(buf,34) = vd->cloth_color;
	WBUFW(buf,36) = (sd)? sd->head_dir : 0;
#if PACKETVER >= 20110111
	WBUFW(buf,38) = vd->robe;
	offset+= 2;
	buf = WBUFP(buffer,offset);
#endif
	WBUFL(buf,38) = clif_visual_guild_id(bl);
	WBUFW(buf,42) = clif_visual_emblem_id(bl);
	WBUFW(buf,44) = (sd)? sd->status.manner : 0;
#if PACKETVER < 7
	WBUFW(buf,46) = (sc)? sc->opt3 : 0;
#else
	WBUFL(buf,46) = (sc)? sc->opt3 : 0;
	offset+=2; //Shift the rest of elements by 2 bytes.
	buf = WBUFP(buffer,offset);
#endif
	WBUFB(buf,48) = (sd)? sd->status.karma : 0;
	WBUFB(buf,49) = vd->sex;
	WBUFPOS2(buf,50,bl->x,bl->y,ud->to_x,ud->to_y,8,8);
	WBUFB(buf,56) = (sd)? 5 : 0;
	WBUFB(buf,57) = (sd)? 5 : 0;
	WBUFW(buf,58) = clif_setlevel(bl);
#if PACKETVER >= 20080102
	WBUFW(buf,60) = sd?sd->user_font:0;
#endif
#if PACKETVER >= 20091103
	strcpy((char*)WBUFP(buf,62), name);
	return WBUFW(buffer,2);
#else
	return packet_len(WBUFW(buffer,0));
#endif
}

//Modifies the buffer for disguise characters and sends it to self.
//Used for spawn/walk packets, where the ID offset changes for packetver >=9
static void clif_setdisguise(struct block_list *bl, unsigned char *buf,int len)
{
#if PACKETVER >= 20091103
	WBUFB(buf,4)= pcdb_checkid(status_get_viewdata(bl)->class_) ? 0x0 : 0x5; //PC_TYPE : NPC_MOB_TYPE
	WBUFL(buf,5)=-bl->id;
#elif PACKETVER >= 20071106
	WBUFB(buf,2)= pcdb_checkid(status_get_viewdata(bl)->class_) ? 0x0 : 0x5; //PC_TYPE : NPC_MOB_TYPE
	WBUFL(buf,3)=-bl->id;
#else
	WBUFL(buf,2)=-bl->id;
#endif
	clif_send(buf, len, bl, SELF);
}

/*==========================================
 * �N���X�`�F���W type��Mob�̏ꍇ��1�ő���0�H
 *------------------------------------------*/
int clif_class_change(struct block_list *bl,int class_,int type)
{
	unsigned char buf[16];

	nullpo_ret(bl);

	if(!pcdb_checkid(class_)) {
		WBUFW(buf,0)=0x1b0;
		WBUFL(buf,2)=bl->id;
		WBUFB(buf,6)=type;
		WBUFL(buf,7)=class_;
		clif_send(buf,packet_len(0x1b0),bl,AREA);
	}
	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
static void clif_spiritball_single(int fd, struct map_session_data *sd)
{
	WFIFOHEAD(fd, packet_len(0x1e1));
	WFIFOW(fd,0)=0x1e1;
	WFIFOL(fd,2)=sd->bl.id;
	WFIFOW(fd,6)=sd->spiritball;
	WFIFOSET(fd, packet_len(0x1e1));
}

/*==========================================
 *
 *------------------------------------------*/
static void clif_weather_check(struct map_session_data *sd)
{
	int m = sd->bl.m, fd = sd->fd;
	
	if (map[m].flag.snow
		|| map[m].flag.clouds
		|| map[m].flag.fog
		|| map[m].flag.fireworks
		|| map[m].flag.sakura
		|| map[m].flag.leaves
		|| map[m].flag.rain
		|| map[m].flag.clouds2)
	{
		if (map[m].flag.snow)
			clif_specialeffect_single(&sd->bl, 162, fd);
		if (map[m].flag.clouds)
			clif_specialeffect_single(&sd->bl, 233, fd);
		if (map[m].flag.clouds2)
			clif_specialeffect_single(&sd->bl, 516, fd);
		if (map[m].flag.fog)
			clif_specialeffect_single(&sd->bl, 515, fd);
		if (map[m].flag.fireworks) {
			clif_specialeffect_single(&sd->bl, 297, fd);
			clif_specialeffect_single(&sd->bl, 299, fd);
			clif_specialeffect_single(&sd->bl, 301, fd);
		}
		if (map[m].flag.sakura)
			clif_specialeffect_single(&sd->bl, 163, fd);
		if (map[m].flag.leaves)
			clif_specialeffect_single(&sd->bl, 333, fd);
		if (map[m].flag.rain)
			clif_specialeffect_single(&sd->bl, 161, fd);
	}
}

void clif_weather(int m)
{
	struct s_mapiterator* iter;
	struct map_session_data *sd=NULL;

	iter = mapit_getallusers();
	for( sd = (struct map_session_data*)mapit_first(iter); mapit_exists(iter); sd = (struct map_session_data*)mapit_next(iter) )
	{
		if( sd->bl.m == m )
			clif_weather_check(sd);
	}
	mapit_free(iter);
}

int clif_spawn(struct block_list *bl)
{
	unsigned char buf[128];
	struct view_data *vd;
	int len;

	vd = status_get_viewdata(bl);
	if( !vd || vd->class_ == INVISIBLE_CLASS )
		return 0;

	if( battle_config.anti_mayapurple_hack && bl->type == BL_PC )
	{
		TBL_PC *tsd = ((TBL_PC*)bl);
		TBL_PC *sd  = BL_CAST(BL_PC, bl);
		if( tsd->sc.option&(OPTION_HIDE|OPTION_CLOAK) && !tsd->state.evade_antiwpefilter && !sd->special_state.intravision && !sd->sc.data[SC_INTRAVISION] )
			return 0;
	}
	len = clif_set_unit_idle(bl, buf,true);
	clif_send(buf, len, bl, AREA_WOS);
	if (disguised(bl))
		clif_setdisguise(bl, buf, len);

	if (vd->cloth_color)
		clif_refreshlook(bl,bl->id,LOOK_CLOTHES_COLOR,vd->cloth_color,AREA_WOS);
		
	switch (bl->type)
	{
	case BL_PC:
		{
			TBL_PC *sd = ((TBL_PC*)bl);
			if (sd->spiritball > 0)
				clif_spiritball(sd);
			if(sd->state.size==2) // tiny/big players [Valaris]
				clif_specialeffect(bl,423,AREA);
			else if(sd->state.size==1)
				clif_specialeffect(bl,421,AREA);
			if( sd->sc.count )
			{
				if( sd->sc.data[SC_BANDING] )
					clif_status_change(&sd->bl,SI_BANDING,1,9999,sd->sc.data[SC_BANDING]->val1,0,0);
				if( sd->sc.data[SC_ALL_RIDING] )
					clif_status_change(&sd->bl,SI_ALL_RIDING,1,9999,0,0,0);
			}
			if( !battle_config.bg_eAmod_mode && sd->bg_id && map[sd->bl.m].flag.battleground )
				clif_sendbgemblem_area(sd);
			clif_sendauras((TBL_PC*)bl, AREA);
		}
		break;
	case BL_MOB:
		{
			TBL_MOB *md = ((TBL_MOB*)bl);
			if(md->special_state.size==2) // tiny/big mobs [Valaris]
				clif_specialeffect(&md->bl,423,AREA);
			else if(md->special_state.size==1)
				clif_specialeffect(&md->bl,421,AREA);
			if (md->option.hp_show == 1)
				clif_mobhpmeter(md);
		}
		break;
	case BL_NPC:
		{
			TBL_NPC *nd = ((TBL_NPC*)bl);
			if( nd->size == 2 )
				clif_specialeffect(&nd->bl,423,AREA);
			else if( nd->size == 1 )
				clif_specialeffect(&nd->bl,421,AREA);
		}
		break;
	case BL_PET:
		if (vd->head_bottom)
			clif_pet_equip_area((TBL_PET*)bl); // needed to display pet equip properly
		break;
	}
	return 0;
}

//[orn]
int clif_hominfo(struct map_session_data *sd, struct homun_data *hd, int flag)
{
	struct status_data *status;
	unsigned char buf[128];
	
	nullpo_ret(hd);

	status = &hd->battle_status;
	memset(buf,0,packet_len(0x22e));
	WBUFW(buf,0)=0x22e;
	memcpy(WBUFP(buf,2),hd->homunculus.name,NAME_LENGTH);
	// Bit field, bit 0 : rename_flag (1 = already renamed), bit 1 : homunc vaporized (1 = true), bit 2 : homunc dead (1 = true)
	WBUFB(buf,26)=(battle_config.hom_rename?0:hd->homunculus.rename_flag) | (hd->homunculus.vaporize << 1) | (hd->homunculus.hp?0:4);
	WBUFW(buf,27)=hd->homunculus.level;
	WBUFW(buf,29)=hd->homunculus.hunger;
	WBUFW(buf,31)=(unsigned short) (hd->homunculus.intimacy / 100) ;
	WBUFW(buf,33)=0; // equip id
	WBUFW(buf,35)=cap_value(status->rhw.atk2+status->batk, 0, INT16_MAX);
	if( battle_config.renewal_system_enable )
		WBUFW(buf,37)=status->matk;
	else
		WBUFW(buf,37)=cap_value(status->matk_max, 0, INT16_MAX);
	WBUFW(buf,39)=status->hit;
	if (battle_config.hom_setting&0x10)
		WBUFW(buf,41)=status->luk/3 + 1;	//crit is a +1 decimal value! Just display purpose.[Vicious]
	else
		WBUFW(buf,41)=status->cri/10;
	WBUFW(buf,43)=status->def + status->vit ;
	WBUFW(buf,45)=status->mdef;
	WBUFW(buf,47)=status->flee;
	WBUFW(buf,49)=(flag)?0:status->amotion;
	if (status->max_hp > INT16_MAX) {
		WBUFW(buf,51) = status->hp/(status->max_hp/100);
		WBUFW(buf,53) = 100;
	} else {
		WBUFW(buf,51)=status->hp;
		WBUFW(buf,53)=status->max_hp;
	}
	if (status->max_sp > INT16_MAX) {
		WBUFW(buf,55) = status->sp/(status->max_sp/100);
		WBUFW(buf,57) = 100;
	} else {
		WBUFW(buf,55)=status->sp;
		WBUFW(buf,57)=status->max_sp;
	}
	WBUFL(buf,59)=hd->homunculus.exp;
	WBUFL(buf,63)=hd->exp_next;
	WBUFW(buf,67)=hd->homunculus.skillpts;
	WBUFW(buf,69)=status_get_range(&hd->bl);
	clif_send(buf,packet_len(0x22e),&sd->bl,SELF);
	return 0;
}

void clif_send_homdata(struct map_session_data *sd, int type, int param)
{	//[orn]
	int fd = sd->fd;
	WFIFOHEAD(fd, packet_len(0x230));
	nullpo_retv(sd->hd);
	WFIFOW(fd,0)=0x230;
	WFIFOW(fd,2)=type;  // FIXME: This is actually <type>.B <state>.B
	WFIFOL(fd,4)=sd->hd->bl.id;
	WFIFOL(fd,8)=param;
	WFIFOSET(fd,packet_len(0x230));

	return;
}

int clif_homskillinfoblock(struct map_session_data *sd)
{	//[orn]
	struct homun_data *hd;
	int fd = sd->fd;
	int i,j,len=4,id;
	WFIFOHEAD(fd, 4+37*MAX_HOMUNSKILL);

	hd = sd->hd;
	if ( !hd ) 
		return 0 ;

	WFIFOW(fd,0)=0x235;
	for ( i = 0; i < MAX_HOMUNSKILL; i++){
		if( (id = hd->homunculus.hskill[i].id) != 0 ){
			j = id - HM_SKILLBASE;
			WFIFOW(fd,len  ) = id;
			WFIFOW(fd,len+2) = skill_get_inf(id);
			WFIFOW(fd,len+4) = 0;
			WFIFOW(fd,len+6) = hd->homunculus.hskill[j].lv;
			WFIFOW(fd,len+8) = skill_get_sp(id,hd->homunculus.hskill[j].lv);
			WFIFOW(fd,len+10)= skill_get_range2(&sd->hd->bl, id,hd->homunculus.hskill[j].lv);
			safestrncpy((char*)WFIFOP(fd,len+12), skill_get_name(id), NAME_LENGTH);
			WFIFOB(fd,len+36) = (hd->homunculus.hskill[j].lv < merc_skill_tree_get_max(id, hd->homunculus.class_))?1:0;
			len+=37;
		}
	}
	WFIFOW(fd,2)=len;
	WFIFOSET(fd,len);

	return 0;
}

void clif_homskillup(struct map_session_data *sd, int skill_num)
{	//[orn]
	struct homun_data *hd;
	int fd, skillid;
	nullpo_retv(sd);
	skillid = skill_num - HM_SKILLBASE;

	fd=sd->fd;
	hd=sd->hd;

	WFIFOHEAD(fd, packet_len(0x239));
	WFIFOW(fd,0) = 0x239;
	WFIFOW(fd,2) = skill_num;
	WFIFOW(fd,4) = hd->homunculus.hskill[skillid].lv;
	WFIFOW(fd,6) = skill_get_sp(skill_num,hd->homunculus.hskill[skillid].lv);
	WFIFOW(fd,8) = skill_get_range2(&hd->bl, skill_num,hd->homunculus.hskill[skillid].lv);
	WFIFOB(fd,10) = (hd->homunculus.hskill[skillid].lv < skill_get_max(hd->homunculus.hskill[skillid].id)) ? 1 : 0;
	WFIFOSET(fd,packet_len(0x239));

	return;
}

int clif_hom_food(struct map_session_data *sd,int foodid,int fail)	//[orn]
{
	int fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x22f));
	WFIFOW(fd,0)=0x22f;
	WFIFOB(fd,2)=fail;
	WFIFOW(fd,3)=foodid;
	WFIFOSET(fd,packet_len(0x22f));

	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_walkok(struct map_session_data *sd)
{
	int fd=sd->fd;
	WFIFOHEAD(fd, packet_len(0x87));
	WFIFOW(fd,0)=0x87;
	WFIFOL(fd,2)=gettick();
	WFIFOPOS2(fd,6,sd->bl.x,sd->bl.y,sd->ud.to_x,sd->ud.to_y,8,8);
	WFIFOSET(fd,packet_len(0x87));

	return 0;
}

static void clif_move2(struct block_list *bl, struct view_data *vd, struct unit_data *ud)
{
	uint8 buf[128];
	struct map_session_data *sd = BL_CAST(BL_PC,bl);
	int len, flag = 0;

	len = clif_set_unit_walking(bl,ud,buf);
	if( battle_config.anti_mayapurple_hack && sd && sd->sc.option&(OPTION_HIDE|OPTION_CLOAK) && !sd->state.evade_antiwpefilter )
		flag = 1;
	clif_send(buf,len,bl,(flag ? AREA_IWOS : AREA_WOS));
	if( disguised(bl) )
		clif_setdisguise(bl, buf, len);
		
	if( vd->cloth_color )
		clif_refreshlook(bl,bl->id,LOOK_CLOTHES_COLOR,vd->cloth_color,(flag ? AREA_IWOS : AREA_WOS));

	switch(bl->type)
	{
	case BL_PC:
		{
			TBL_PC *sd = ((TBL_PC*)bl);
//			clif_movepc(sd);
			if(sd->state.size==2) // tiny/big players [Valaris]
				clif_specialeffect(&sd->bl,423,(flag ? AREA_IWS : AREA));
			else if(sd->state.size==1)
				clif_specialeffect(&sd->bl,421,(flag ? AREA_IWS : AREA));
		}
		break;
	case BL_MOB:
		{
			TBL_MOB *md = ((TBL_MOB*)bl);
			if(md->special_state.size==2) // tiny/big mobs [Valaris]
				clif_specialeffect(&md->bl,423,AREA);
			else if(md->special_state.size==1)
				clif_specialeffect(&md->bl,421,AREA);
		}
		break;
	case BL_PET:
		if( vd->head_bottom )
		{// needed to display pet equip properly
			clif_pet_equip_area((TBL_PET*)bl); 
		}
		break;
	}
	return;
}

/// Move the unit (does nothing if the client has no info about the unit)
/// Note: unit must not be self
void clif_move(struct unit_data *ud)
{
	unsigned char buf[16];
	struct view_data* vd;
	struct block_list* bl = ud->bl;
	struct map_session_data *tsd = BL_CAST(BL_PC,bl);
	int target = AREA_WOS;

	vd = status_get_viewdata(bl);
	if (!vd || vd->class_ == INVISIBLE_CLASS)
		return; //This performance check is needed to keep GM-hidden objects from being notified to bots.
	
	if (ud->state.speed_changed) {
		// Since we don't know how to update the speed of other objects,
		// use the old walk packet to update the data.
		ud->state.speed_changed = 0;
		clif_move2(bl, vd, ud);
		return;
	}

	if( battle_config.anti_mayapurple_hack && tsd && tsd->sc.option&(OPTION_HIDE|OPTION_CLOAK) && !tsd->state.evade_antiwpefilter )
		target = AREA_IWOS;

	WBUFW(buf,0)=0x86;
	WBUFL(buf,2)=bl->id;
	WBUFPOS2(buf,6,bl->x,bl->y,ud->to_x,ud->to_y,8,8);
	WBUFL(buf,12)=gettick();
	clif_send(buf, 16, bl, target);
	if( disguised(bl) )
	{
		WBUFL(buf,2)=-bl->id;
		clif_send(buf, 16, bl, SELF);
	}
}

/*==========================================
 * Delays the map_quit of a player after they are disconnected. [Skotlex]
 *------------------------------------------*/
static int clif_delayquit(int tid, unsigned int tick, int id, intptr_t data)
{
	struct map_session_data *sd = NULL;

	//Remove player from map server
	if ((sd = map_id2sd(id)) != NULL && sd->fd == 0) //Should be a disconnected player.
		map_quit(sd);
	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
void clif_quitsave(int fd,struct map_session_data *sd)
{
	if (!battle_config.prevent_logout ||
	  	DIFF_TICK(gettick(), sd->canlog_tick) > battle_config.prevent_logout)
		map_quit(sd);
	else if (sd->fd)
	{	//Disassociate session from player (session is deleted after this function was called)
		//And set a timer to make him quit later.
		session[sd->fd]->session_data = NULL;
		sd->fd = 0;
		add_timer(gettick() + 10000, clif_delayquit, sd->bl.id, 0);
	}
}

/*==========================================
 *
 *------------------------------------------*/
void clif_changemap(struct map_session_data *sd, short map, int x, int y)
{
	int fd;
	nullpo_retv(sd);
	fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x91));
	WFIFOW(fd,0) = 0x91;
	mapindex_getmapname_ext(mapindex_id2name(map), (char*)WFIFOP(fd,2));
	WFIFOW(fd,18) = x;
	WFIFOW(fd,20) = y;
	WFIFOSET(fd,packet_len(0x91));

	// AutoRefresh - Restart
	if( sd->sc.data[SC_AUTOREFRESH] && sd->sc.data[SC_AUTOREFRESH]->val2 == 0 )
		sc_start2(&sd->bl,SC_AUTOREFRESH,100,sd->sc.data[SC_AUTOREFRESH]->val1,0,-1);
}

/*==========================================
 * Tells the client to connect to another map-server
 *------------------------------------------*/
void clif_changemapserver(struct map_session_data* sd, unsigned short map_index, int x, int y, uint32 ip, uint16 port)
{
	int fd;
	nullpo_retv(sd);
	fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x92));
	WFIFOW(fd,0) = 0x92;
	mapindex_getmapname_ext(mapindex_id2name(map_index), (char*)WFIFOP(fd,2));
	WFIFOW(fd,18) = x;
	WFIFOW(fd,20) = y;
	WFIFOL(fd,22) = htonl(ip);
	WFIFOW(fd,26) = ntows(htons(port)); // [!] LE byte order here [!]
	WFIFOSET(fd,packet_len(0x92));
}

void clif_blown(struct block_list *bl)
{
	clif_fixpos(bl);
}

void clif_blown_slide(struct block_list *bl)
{
	clif_slide(bl, bl->x, bl->y);
}

/// Visually moves(slides) a character to x,y. If the target cell
/// isn't walkable, the char doesn't move at all. If the char is
/// sitting it will stand up.
/// S 0088 <gid>.L <x>.W <y>.W
void clif_fixpos(struct block_list *bl)
{
	unsigned char buf[10];
	nullpo_retv(bl);

	WBUFW(buf,0) = 0x88;
	WBUFL(buf,2) = bl->id;
	WBUFW(buf,6) = bl->x;
	WBUFW(buf,8) = bl->y;
	clif_send(buf, packet_len(0x88), bl, AREA);

	if( disguised(bl) )
	{
		WBUFL(buf,2) = -bl->id;
		clif_send(buf, packet_len(0x88), bl, SELF);
	}
}

/*==========================================
 *
 *------------------------------------------*/
int clif_npcbuysell(struct map_session_data* sd, int id)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len(0xc4));
	WFIFOW(fd,0)=0xc4;
	WFIFOL(fd,2)=id;
	WFIFOSET(fd,packet_len(0xc4));

	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_buylist(struct map_session_data *sd, struct npc_data *nd)
{
	int fd,i,c;

	nullpo_ret(sd);
	nullpo_ret(nd);

	fd = sd->fd;
 	WFIFOHEAD(fd, 4 + nd->u.shop.count * 11);
	WFIFOW(fd,0) = 0xc6;

	c = 0;
	for( i = 0; i < nd->u.shop.count; i++ )
	{
		struct item_data* id = itemdb_exists(nd->u.shop.shop_item[i].nameid);
		int val = nd->u.shop.shop_item[i].value;
		if( id == NULL )
			continue;
		WFIFOL(fd, 4+c*11) = val;
		WFIFOL(fd, 8+c*11) = pc_modifybuyvalue(sd,val);
		WFIFOB(fd,12+c*11) = itemtype(id->type);
		WFIFOW(fd,13+c*11) = ( id->view_id > 0 ) ? id->view_id : id->nameid;
		c++;
	}

	WFIFOW(fd,2) = 4 + c*11;
	WFIFOSET(fd,WFIFOW(fd,2));

	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_selllist(struct map_session_data *sd)
{
	int fd, i, c = 0, val, char_id;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd, MAX_INVENTORY * 10 + 4);
	WFIFOW(fd,0)=0xc7;
	for( i = 0; i < MAX_INVENTORY; i++ )
	{
		if( sd->status.inventory[i].nameid > 0 && sd->inventory_data[i] )
		{
			if( !itemdb_cansell(&sd->status.inventory[i], pc_isGM(sd)) )
				continue;

			if( sd->status.inventory[i].expire_time || sd->status.inventory[i].bound )
				continue; // Cannot Sell Rental Items - Account Bound

			if( sd->status.inventory[i].card[0] == CARD0_CREATE )
			{ // Do not allow sell BG - Ancient Items
				char_id = MakeDWord(sd->status.inventory[i].card[2],sd->status.inventory[i].card[3]);
				if( battle_config.bg_reserved_char_id && char_id == battle_config.bg_reserved_char_id )
					continue;
				if( battle_config.woe_reserved_char_id && char_id == battle_config.woe_reserved_char_id )
					continue;
				if( battle_config.ancient_reserved_char_id && char_id == battle_config.ancient_reserved_char_id )
					continue;
				if( battle_config.hunting_reserved_char_id && char_id == battle_config.hunting_reserved_char_id )
					continue;
			}

			val=sd->inventory_data[i]->value_sell;
			if( val < 0 )
				continue;
			WFIFOW(fd,4+c*10)=i+2;
			WFIFOL(fd,6+c*10)=val;
			WFIFOL(fd,10+c*10)=pc_modifysellvalue(sd,val);
			c++;
		}
	}
	WFIFOW(fd,2)=c*10+4;
	WFIFOSET(fd,WFIFOW(fd,2));

	return 0;
}

/// Client behavior (dialog window):
/// - disable mouse targeting
/// - open the dialog window
/// - set npcid of dialog window (0 by default)
/// - if set to clear on next mes, clear contents
/// - append this text
int clif_scriptmes(struct map_session_data *sd, int npcid, const char *mes)
{
	int fd = sd->fd;
	int slen = strlen(mes) + 9;
	WFIFOHEAD(fd, slen);
	WFIFOW(fd,0)=0xb4;
	WFIFOW(fd,2)=slen;
	WFIFOL(fd,4)=npcid;
	strcpy((char*)WFIFOP(fd,8),mes);
	WFIFOSET(fd,WFIFOW(fd,2));

	return 0;
}

/// Client behavior (dialog window):
/// - disable mouse targeting
/// - open the dialog window
/// - add 'next' button
/// When 'next' is pressed:
/// - S 00B9 <npcid of dialog window>.L
/// - set to clear on next mes
/// - remove 'next' button
int clif_scriptnext(struct map_session_data *sd,int npcid)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len(0xb5));
	WFIFOW(fd,0)=0xb5;
	WFIFOL(fd,2)=npcid;
	WFIFOSET(fd,packet_len(0xb5));

	return 0;
}

/// Client behavior:
/// - if dialog window is open:
///   - remove 'next' button
///   - add 'close' button
/// - else:
///   - enable mouse targeting
///   - close the dialog window
///   - close the menu window
/// When 'close' is pressed:
/// - enable mouse targeting
/// - close the dialog window
/// - close the menu window
/// - S 0146 <npcid of dialog window>.L
int clif_scriptclose(struct map_session_data *sd, int npcid)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len(0xb6));
	WFIFOW(fd,0)=0xb6;
	WFIFOL(fd,2)=npcid;
	WFIFOSET(fd,packet_len(0xb6));

	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
void clif_sendfakenpc(struct map_session_data *sd, int npcid)
{
	unsigned char *buf;
	int fd = sd->fd;
	sd->state.using_fake_npc = 1;

	WFIFOHEAD(fd, packet_len(0x78));
	buf = WFIFOP(fd,0);
	memset(WBUFP(buf,0), 0, packet_len(0x78));
	WBUFW(buf,0)=0x78;
#if PACKETVER >= 20071106
	WBUFB(buf,2) = 0; //Unknown bit
	buf = WFIFOP(fd,1);
#endif
	WBUFL(buf,2)=npcid;
	WBUFW(buf,14)=111;
	WBUFPOS(buf,46,sd->bl.x,sd->bl.y,sd->ud.dir);
	WBUFB(buf,49)=5;
	WBUFB(buf,50)=5;
	WFIFOSET(fd, packet_len(0x78));

	return;
}

/// Client behavior:
/// - disable mouse targeting
/// - close the menu window
/// - open the menu window
/// - add options to the menu (separated in the text by ":")
/// - set npcid of menu window
/// - if dialog window is open:
///   - remove 'next' button
/// When 'ok' is pressed:
/// - S 00B8 <npcid of menu window>.L <selected option>.B
/// - close the menu window
/// When 'cancel' is pressed:
/// - S 00B8 <npcid of menu window>.L <-1>.B
/// - enable mouse targeting
/// - close a bunch of windows...
/// WARNING: the 'cancel' button closes other windows besides the dialog window and the menu window.
///    Which suggests their have intertwined behavior. (probably the mouse targeting)
/// TODO investigate behavior of other windows [FlavioJS]
int clif_scriptmenu(struct map_session_data* sd, int npcid, const char* mes)
{
	int fd = sd->fd;
	int slen = strlen(mes) + 8;
	struct block_list *bl = NULL;

	if (!sd->state.using_fake_npc && (npcid == fake_nd->bl.id || ((bl = map_id2bl(npcid)) && (bl->m!=sd->bl.m ||
	   bl->x<sd->bl.x-AREA_SIZE-1 || bl->x>sd->bl.x+AREA_SIZE+1 ||
	   bl->y<sd->bl.y-AREA_SIZE-1 || bl->y>sd->bl.y+AREA_SIZE+1))))
	   clif_sendfakenpc(sd, npcid);

	WFIFOHEAD(fd, slen);
	WFIFOW(fd,0)=0xb7;
	WFIFOW(fd,2)=slen;
	WFIFOL(fd,4)=npcid;
	strcpy((char*)WFIFOP(fd,8),mes);
	WFIFOSET(fd,WFIFOW(fd,2));
	
	return 0;
}

/// Client behavior (inputnum window):
/// - if npcid exists in the client:
///   - open the inputnum window
///   - set npcid of inputnum window
/// When 'ok' is pressed:
/// - if inputnum window has text:
///   - if npcid exists in the client:
///     - S 0143 <npcid of inputnum window>.L <atoi(text)>.L
///   - close inputnum window
int clif_scriptinput(struct map_session_data *sd, int npcid)
{
	int fd;
	struct block_list *bl = NULL;

	nullpo_ret(sd);

	if (!sd->state.using_fake_npc && (npcid == fake_nd->bl.id || ((bl = map_id2bl(npcid)) && (bl->m!=sd->bl.m ||
	   bl->x<sd->bl.x-AREA_SIZE-1 || bl->x>sd->bl.x+AREA_SIZE+1 ||
	   bl->y<sd->bl.y-AREA_SIZE-1 || bl->y>sd->bl.y+AREA_SIZE+1))))
	   clif_sendfakenpc(sd, npcid);
	
	fd=sd->fd;
	WFIFOHEAD(fd, packet_len(0x142));
	WFIFOW(fd,0)=0x142;
	WFIFOL(fd,2)=npcid;
	WFIFOSET(fd,packet_len(0x142));

	return 0;
}

/// Client behavior (inputstr window):
/// - if npcid is 0 or npcid exists in the client:
///   - open the inputstr window
///   - set npcid of inputstr window
/// When 'ok' is pressed:
/// - if inputstr window has text and isn't an insult(manner.txt):
///   - if npcid is 0 or npcid exists in the client:
///     - S 01d5 <packetlen>.W <npcid of inputstr window>.L <text>.?B
///   - close inputstr window
int clif_scriptinputstr(struct map_session_data *sd, int npcid)
{
	int fd;
	struct block_list *bl = NULL;

	nullpo_ret(sd);

	if (!sd->state.using_fake_npc && (npcid == fake_nd->bl.id || ((bl = map_id2bl(npcid)) && (bl->m!=sd->bl.m ||
	   bl->x<sd->bl.x-AREA_SIZE-1 || bl->x>sd->bl.x+AREA_SIZE+1 ||
	   bl->y<sd->bl.y-AREA_SIZE-1 || bl->y>sd->bl.y+AREA_SIZE+1))))
	   clif_sendfakenpc(sd, npcid);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len(0x1d4));
	WFIFOW(fd,0)=0x1d4;
	WFIFOL(fd,2)=npcid;
	WFIFOSET(fd,packet_len(0x1d4));
	
	return 0;
}

/// npc_id is ignored in the client
/// type=2     : Remove viewpoint
/// type=other : Show viewpoint
int clif_viewpoint(struct map_session_data *sd, int npc_id, int type, int x, int y, int id, int color)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len(0x144));
	WFIFOW(fd,0)=0x144;
	WFIFOL(fd,2)=npc_id;
	WFIFOL(fd,6)=type;
	WFIFOL(fd,10)=x;
	WFIFOL(fd,14)=y;
	WFIFOB(fd,18)=id;
	WFIFOL(fd,19)=color;
	WFIFOSET(fd,packet_len(0x144));

	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_cutin(struct map_session_data* sd, const char* image, int type)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len(0x1b3));
	WFIFOW(fd,0)=0x1b3;
	strncpy((char*)WFIFOP(fd,2),image,64);
	WFIFOB(fd,66)=type;
	WFIFOSET(fd,packet_len(0x1b3));

	return 0;
}

/*==========================================
 * Fills in card data from the given item and into the buffer. [Skotlex]
 *------------------------------------------*/
static void clif_addcards(unsigned char* buf, struct item* item)
{
	int i=0,j;
	if( item == NULL ) { //Blank data
		WBUFW(buf,0) = 0;
		WBUFW(buf,2) = 0;
		WBUFW(buf,4) = 0;
		WBUFW(buf,6) = 0;
		return;
	}
	if( item->card[0] == CARD0_PET ) { //pet eggs
		WBUFW(buf,0) = 0;
		WBUFW(buf,2) = 0;
		WBUFW(buf,4) = 0;
		WBUFW(buf,6) = item->card[3]; //Pet renamed flag.
		return;
	}
	if( item->card[0] == CARD0_FORGE || item->card[0] == CARD0_CREATE ) { //Forged/created items
		WBUFW(buf,0) = item->card[0];
		WBUFW(buf,2) = item->card[1];
		WBUFW(buf,4) = item->card[2];
		WBUFW(buf,6) = item->card[3];
		return;
	}
	//Client only receives four cards.. so randomly send them a set of cards. [Skotlex]
	if( MAX_SLOTS > 4 && (j = itemdb_slot(item->nameid)) > 4 )
		i = rand()%(j-3); //eg: 6 slots, possible i values: 0->3, 1->4, 2->5 => i = rand()%3;

	//Normal items.
	if( item->card[i] > 0 && (j=itemdb_viewid(item->card[i])) > 0 )
		WBUFW(buf,0) = j;
	else
		WBUFW(buf,0) = item->card[i];

	if( item->card[++i] > 0 && (j=itemdb_viewid(item->card[i])) > 0 )
		WBUFW(buf,2) = j;
	else
		WBUFW(buf,2) = item->card[i];

	if( item->card[++i] > 0 && (j=itemdb_viewid(item->card[i])) > 0 )
		WBUFW(buf,4) = j;
	else
		WBUFW(buf,4) = item->card[i];

	if( item->card[++i] > 0 && (j=itemdb_viewid(item->card[i])) > 0 )
		WBUFW(buf,6) = j;
	else
		WBUFW(buf,6) = item->card[i];
}

/*==========================================
 *
 *------------------------------------------*/
int clif_additem(struct map_session_data *sd, int n, int amount, int fail)
{
	int fd;
#if PACKETVER < 20071002
	const int cmd = 0xa0;
#else
	const int cmd = 0x2d4;
#endif
	nullpo_ret(sd);

	fd = sd->fd;
	if( !session_isActive(fd) )  //Sasuke-
		return 0;

	WFIFOHEAD(fd,packet_len(cmd));
	if( fail )
	{
		WFIFOW(fd,0)=cmd;
		WFIFOW(fd,2)=n+2;
		WFIFOW(fd,4)=amount;
		WFIFOW(fd,6)=0;
		WFIFOB(fd,8)=0;
		WFIFOB(fd,9)=0;
		WFIFOB(fd,10)=0;
		WFIFOW(fd,11)=0;
		WFIFOW(fd,13)=0;
		WFIFOW(fd,15)=0;
		WFIFOW(fd,17)=0;
		WFIFOW(fd,19)=0;
		WFIFOB(fd,21)=0;
		WFIFOB(fd,22)=fail;
#if PACKETVER >= 20071002
		WFIFOL(fd,23)=0;
		WFIFOW(fd,27)=0;
#endif
	}
	else
	{
		if( n < 0 || n >= MAX_INVENTORY || sd->status.inventory[n].nameid <=0 || sd->inventory_data[n] == NULL )
			return 1;

		WFIFOW(fd,0)=cmd;
		WFIFOW(fd,2)=n+2;
		WFIFOW(fd,4)=amount;
		if (sd->inventory_data[n]->view_id > 0)
			WFIFOW(fd,6)=sd->inventory_data[n]->view_id;
		else
			WFIFOW(fd,6)=sd->status.inventory[n].nameid;
		WFIFOB(fd,8)=sd->status.inventory[n].identify;
		WFIFOB(fd,9)=sd->status.inventory[n].attribute;
		WFIFOB(fd,10)=sd->status.inventory[n].refine;
		clif_addcards(WFIFOP(fd,11), &sd->status.inventory[n]);
		WFIFOW(fd,19)=pc_equippoint(sd,n);
		WFIFOB(fd,21)=itemtype(sd->inventory_data[n]->type);
		WFIFOB(fd,22)=fail;
#if PACKETVER >= 20071002
		WFIFOL(fd,23)=sd->status.inventory[n].expire_time;
		WFIFOW(fd,27)=sd->status.inventory[n].bound ? 2 : 0;
#endif
	}

	WFIFOSET(fd,packet_len(cmd));
	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_dropitem(struct map_session_data *sd,int n,int amount)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len(0xaf));
	WFIFOW(fd,0)=0xaf;
	WFIFOW(fd,2)=n+2;
	WFIFOW(fd,4)=amount;
	WFIFOSET(fd,packet_len(0xaf));

	return 0;
}

/*==========================================
 * Deletes an item from your inventory.
 * Reason values:
 * 0 - Normal
 * 1 - Item used for a skill
 * 2 - Refine failed
 * 3 - Material changed
 * 4 - Moved to storage
 * 5 - Moved to cart
 * 6 - Item sold
 * 7 - Item dropped 
 *------------------------------------------*/
int clif_delitem(struct map_session_data *sd,int n,int amount, short reason)
{
#if PACKETVER < 20091117
	return clif_dropitem(sd,n,amount);
#else
	int fd;

	nullpo_ret(sd);
	
	if (reason == 7)
		return clif_dropitem(sd,n,amount);

	fd=sd->fd;
	
	WFIFOHEAD(fd, packet_len(0x7fa));
	WFIFOW(fd,0)=0x7fa;
	WFIFOW(fd,2)=reason;
	WFIFOW(fd,4)=n+2;
	WFIFOW(fd,6)=amount;
	WFIFOSET(fd,packet_len(0x7fa));

	return 0;
#endif
}

// Simplifies inventory/cart/storage packets by handling the packet section relevant to items. [Skotlex]
// Equip is >= 0 for equippable items (holds the equip-point, is 0 for pet 
// armor/egg) -1 for stackable items, -2 for stackable items where arrows must send in the equip-point.
void clif_item_sub(unsigned char *buf, int n, struct item *i, struct item_data *id, int equip)
{
	if (id->view_id > 0)
		WBUFW(buf,n)=id->view_id;
	else
		WBUFW(buf,n)=i->nameid;
	WBUFB(buf,n+2)=itemtype(id->type);
	WBUFB(buf,n+3)=i->identify;
	if (equip >= 0) { //Equippable item 
		WBUFW(buf,n+4)=equip;
		WBUFW(buf,n+6)=i->equip;
		WBUFB(buf,n+8)=i->attribute;
		WBUFB(buf,n+9)=i->refine;
	} else { //Stackable item.
		WBUFW(buf,n+4)=i->amount;
		if (equip == -2 && id->equip == EQP_AMMO)
			WBUFW(buf,n+6)=EQP_AMMO;
		else
			WBUFW(buf,n+6)=0;
	}

}

//Unified inventory function which sends all of the inventory (requires two packets, one for equipable items and one for stackable ones. [Skotlex]
void clif_inventorylist(struct map_session_data *sd)
{
	int i,n,ne,arrow=-1;
	unsigned char *buf;
	unsigned char *bufe;

#if PACKETVER < 5
	const int s = 10; //Entry size.
#elif PACKETVER < 20080102
	const int s = 18;
#else
	const int s = 22;
#endif
#if PACKETVER < 20071002
	const int se = 20;
#elif PACKETVER < 20100629
	const int se = 26;
#else
	const int se = 28;
#endif

	buf = (unsigned char*)aMallocA(MAX_INVENTORY * s + 4);
	bufe = (unsigned char*)aMallocA(MAX_INVENTORY * se + 4);
	
	for( i = 0, n = 0, ne = 0; i < MAX_INVENTORY; i++ )
	{
		if( sd->status.inventory[i].nameid <=0 || sd->inventory_data[i] == NULL )
			continue;

		if( !itemdb_isstackable2(sd->inventory_data[i]) )
		{	//Non-stackable (Equippable)
			WBUFW(bufe,ne*se+4)=i+2;
			clif_item_sub(bufe, ne*se+6, &sd->status.inventory[i], sd->inventory_data[i], pc_equippoint(sd,i));
			clif_addcards(WBUFP(bufe, ne*se+16), &sd->status.inventory[i]);
#if PACKETVER >= 20071002
			WBUFL(bufe,ne*se+24)=sd->status.inventory[i].expire_time;
			WBUFW(bufe,ne*se+28)=sd->status.inventory[i].bound ? 2 : 0;
#endif
#if PACKETVER >= 20100629
			if (sd->inventory_data[i]->equip&EQP_VISIBLE)
				WBUFW(bufe,ne*se+30)= sd->inventory_data[i]->look;
			else
				WBUFW(bufe,ne*se+30)=0;
#endif
			ne++;
		}
		else
		{ //Stackable.
			WBUFW(buf,n*s+4)=i+2;
			clif_item_sub(buf, n*s+6, &sd->status.inventory[i], sd->inventory_data[i], -2);
			if( sd->inventory_data[i]->equip == EQP_AMMO && sd->status.inventory[i].equip )
				arrow=i;
#if PACKETVER >= 5
			clif_addcards(WBUFP(buf, n*s+14), &sd->status.inventory[i]);
#endif
#if PACKETVER >= 20080102
			WBUFL(buf,n*s+22)=sd->status.inventory[i].expire_time;
#endif
			n++;
		}
	}
	if( n )
	{
#if PACKETVER < 5
		WBUFW(buf,0)=0xa3;
#elif PACKETVER < 20080102
		WBUFW(buf,0)=0x1ee;
#else
		WBUFW(buf,0)=0x2e8;
#endif
		WBUFW(buf,2)=4+n*s;
		clif_send(buf, WBUFW(buf,2), &sd->bl, SELF);
	}
	if( arrow >= 0 )
		clif_arrowequip(sd,arrow);

	if( ne )
	{
#if PACKETVER < 20071002
		WBUFW(bufe,0)=0xa4;
#else
		WBUFW(bufe,0)=0x2d0;
#endif
		WBUFW(bufe,2)=4+ne*se;
		clif_send(bufe, WBUFW(bufe,2), &sd->bl, SELF);
	}

	if( buf ) aFree(buf);
	if( bufe ) aFree(bufe);
}

//Required when items break/get-repaired. Only sends equippable item list.
void clif_equiplist(struct map_session_data *sd)
{
	int i,n,fd = sd->fd;
	unsigned char *buf;
#if PACKETVER < 20071002
	const int se = 20;
#elif PACKETVER < 20100629
	const int se = 26;
#else
	const int se = 28;
#endif

	WFIFOHEAD(fd, MAX_INVENTORY * se + 4);
	buf = WFIFOP(fd,0);

	for(i=0,n=0;i<MAX_INVENTORY;i++){
		if (sd->status.inventory[i].nameid <=0 || sd->inventory_data[i] == NULL)
			continue;  
	
		if(itemdb_isstackable2(sd->inventory_data[i])) 
			continue;
		//Equippable
		WBUFW(buf,n*se+4)=i+2;
		clif_item_sub(buf, n*se+6, &sd->status.inventory[i], sd->inventory_data[i], pc_equippoint(sd,i));
		clif_addcards(WBUFP(buf, n*se+16), &sd->status.inventory[i]);
#if PACKETVER >= 20071002
		WBUFL(buf,n*se+24)=sd->status.inventory[i].expire_time;
		WBUFW(buf,n*se+28)=sd->status.inventory[i].bound ? 2 : 0;
#endif
#if PACKETVER >= 20100629
		if (sd->inventory_data[i]->equip&EQP_VISIBLE)
			WBUFW(buf,n*se+30)= sd->inventory_data[i]->look;
		else
			WBUFW(buf,n*se+30)=0;
#endif
		n++;
	}
	if (n) {
#if PACKETVER < 20071002
		WBUFW(buf,0)=0xa4;
#else
		WBUFW(buf,0)=0x2d0;
#endif
		WBUFW(buf,2)=4+n*se;
		WFIFOSET(fd,WFIFOW(fd,2));
	}
}

void clif_storagelist(struct map_session_data* sd, struct item* items, int items_length)
{
	struct item_data *id;
	int i,n,ne;
	unsigned char *buf;
	unsigned char *bufe;
#if PACKETVER < 5
	const int s = 10; //Entry size.
#elif PACKETVER < 20080102
	const int s = 18;
#else
	const int s = 22;
#endif
#if PACKETVER < 20071002
	const int se = 20;
#elif PACKETVER < 20100629
	const int se = 26;
#else
	const int se = 28;
#endif

	buf = (unsigned char*)aMallocA(items_length * s + 4);
	bufe = (unsigned char*)aMallocA(items_length * se + 4);

	for( i = 0, n = 0, ne = 0; i < items_length; i++ )
	{
		if( items[i].nameid <= 0 )
			continue;
		id = itemdb_search(items[i].nameid);
		if( !itemdb_isstackable2(id) )
		{ //Equippable
			WBUFW(bufe,ne*se+4)=i+1;
			clif_item_sub(bufe, ne*se+6, &items[i], id, id->equip);
			clif_addcards(WBUFP(bufe, ne*se+16), &items[i]);
#if PACKETVER >= 20071002
			WBUFL(bufe,ne*se+24)=items[i].expire_time;
			WBUFW(bufe,ne*se+28)=items[i].bound ? 2 : 0;
#endif
#if PACKETVER >= 20100629
			if (id->equip&EQP_VISIBLE)
				WBUFW(bufe,ne*se+30)= id->look;
			else
				WBUFW(bufe,ne*se+30)=0;
#endif
			ne++;
		}
		else
		{ //Stackable
			WBUFW(buf,n*s+4)=i+1;
			clif_item_sub(buf, n*s+6, &items[i], id,-1);
#if PACKETVER >= 5
			clif_addcards(WBUFP(buf,n*s+14), &items[i]);
#endif
#if PACKETVER >= 20080102
			WBUFL(buf,n*s+22)=items[i].expire_time;
#endif
			n++;
		}
	}
	if( n )
	{
#if PACKETVER < 5
		WBUFW(buf,0)=0xa5;
#elif PACKETVER < 20080102
		WBUFW(buf,0)=0x1f0;
#else
		WBUFW(buf,0)=0x2ea;
#endif
		WBUFW(buf,2)=4+n*s;
		clif_send(buf, WBUFW(buf,2), &sd->bl, SELF);
	}
	if( ne )
	{
#if PACKETVER < 20071002
		WBUFW(bufe,0)=0xa6;
#else
		WBUFW(bufe,0)=0x2d1;
#endif
		WBUFW(bufe,2)=4+ne*se;
		clif_send(bufe, WBUFW(bufe,2), &sd->bl, SELF);
	}

	if( buf ) aFree(buf);
	if( bufe ) aFree(bufe);
}

void clif_cartlist(struct map_session_data *sd)
{
	struct item_data *id;
	int i,n,ne;
	unsigned char *buf;
	unsigned char *bufe;
#if PACKETVER < 5
	const int s = 10; //Entry size.
#elif PACKETVER < 20080102
	const int s = 18;
#else
	const int s = 22;
#endif
#if PACKETVER < 20071002
	const int se = 20;
#elif PACKETVER < 20100629
	const int se = 26;
#else
	const int se = 28;
#endif

	buf = (unsigned char*)aMallocA(MAX_CART * s + 4);
	bufe = (unsigned char*)aMallocA(MAX_CART * se + 4);
	
	for( i = 0, n = 0, ne = 0; i < MAX_CART; i++ )
	{
		if( sd->status.cart[i].nameid <= 0 )
			continue;
		id = itemdb_search(sd->status.cart[i].nameid);
		if( !itemdb_isstackable2(id) )
		{ //Equippable
			WBUFW(bufe,ne*se+4)=i+2;
			clif_item_sub(bufe, ne*se+6, &sd->status.cart[i], id, id->equip);
			clif_addcards(WBUFP(bufe, ne*se+16), &sd->status.cart[i]);
#if PACKETVER >= 20071002
			WBUFL(bufe,ne*se+24)=sd->status.cart[i].expire_time;
			WBUFW(bufe,ne*se+28)=sd->status.cart[i].bound ? 2 : 0; //Unknown
#endif
#if PACKETVER >= 20100629
			if (id->equip&EQP_VISIBLE)
				WBUFW(bufe,ne*se+30)= id->look;
			else
				WBUFW(bufe,ne*se+30)=0;
#endif
			ne++;
		}
		else
		{ //Stackable
			WBUFW(buf,n*s+4)=i+2;
			clif_item_sub(buf, n*s+6, &sd->status.cart[i], id,-1);
#if PACKETVER >= 5
			clif_addcards(WBUFP(buf,n*s+14), &sd->status.cart[i]);
#endif
#if PACKETVER >= 20080102
			WBUFL(buf,n*s+22)=sd->status.cart[i].expire_time;
#endif
			n++;
		}
	}
	if( n )
	{
#if PACKETVER < 5
		WBUFW(buf,0)=0x123;
#elif PACKETVER < 20080102
		WBUFW(buf,0)=0x1ef;
#else
		WBUFW(buf,0)=0x2e9;
#endif
		WBUFW(buf,2)=4+n*s;
		clif_send(buf, WBUFW(buf,2), &sd->bl, SELF);
	}
	if( ne )
	{
#if PACKETVER < 20071002
		WBUFW(bufe,0)=0x122;
#else
		WBUFW(bufe,0)=0x2d2;
#endif
		WBUFW(bufe,2)=4+ne*se;
		clif_send(bufe, WBUFW(bufe,2), &sd->bl, SELF);
	}

	if( buf ) aFree(buf);
	if( bufe ) aFree(bufe);
}

/// Client behaviour:
/// Closes the cart storage and removes all it's items from memory.
/// The Num & Weight values of the cart are left untouched and the cart is NOT removed.
void clif_clearcart(int fd)
{
	WFIFOHEAD(fd, packet_len(0x12b));
	WFIFOW(fd,0) = 0x12b;
	WFIFOSET(fd, packet_len(0x12b));

}

// Guild XY locators [Valaris]
int clif_guild_xy(struct map_session_data *sd)
{
	unsigned char buf[10];

	nullpo_ret(sd);

	WBUFW(buf,0)=0x1eb;
	WBUFL(buf,2)=sd->status.account_id;
	WBUFW(buf,6)=sd->bl.x;
	WBUFW(buf,8)=sd->bl.y;
	clif_send(buf,packet_len(0x1eb),&sd->bl,GUILD_SAMEMAP_WOS);

	return 0;
}

/*==========================================
 * Sends x/y dot to a single fd. [Skotlex]
 *------------------------------------------*/
int clif_guild_xy_single(int fd, struct map_session_data *sd)
{
	if( sd->bg_id )
		return 0;

	WFIFOHEAD(fd,packet_len(0x1eb));
	WFIFOW(fd,0)=0x1eb;
	WFIFOL(fd,2)=sd->status.account_id;
	WFIFOW(fd,6)=sd->bl.x;
	WFIFOW(fd,8)=sd->bl.y;
	WFIFOSET(fd,packet_len(0x1eb));
	return 0;
}

// Guild XY locators [Valaris]
int clif_guild_xy_remove(struct map_session_data *sd)
{
	unsigned char buf[10];

	nullpo_ret(sd);

	WBUFW(buf,0)=0x1eb;
	WBUFL(buf,2)=sd->status.account_id;
	WBUFW(buf,6)=-1;
	WBUFW(buf,8)=-1;
	clif_send(buf,packet_len(0x1eb),&sd->bl,GUILD_SAMEMAP_WOS);

	return 0;
}

/*==========================================
 * �X�e�[�^�X�𑗂����
 * �\����p�����͂��̒��Ōv�Z���đ���
 *------------------------------------------*/
int clif_updatestatus(struct map_session_data *sd,int type)
{
	int fd,len=8;

	nullpo_ret(sd);

	fd=sd->fd;

	if ( !session_isActive(fd) ) // Invalid pointer fix, by sasuke [Kevin]
		return 0;
 
	WFIFOHEAD(fd, 14);
	WFIFOW(fd,0)=0xb0;
	WFIFOW(fd,2)=type;
	switch(type){
		// 00b0
	case SP_WEIGHT:
		pc_updateweightstatus(sd);
		WFIFOW(fd,0)=0xb0;	//Need to re-set as pc_updateweightstatus can alter the buffer. [Skotlex]
		WFIFOW(fd,2)=type;
		WFIFOL(fd,4)=sd->weight;
		break;
	case SP_MAXWEIGHT:
		WFIFOL(fd,4)=sd->max_weight;
		break;
	case SP_SPEED:
		WFIFOL(fd,4)=sd->battle_status.speed;
		break;
	case SP_BASELEVEL:
		WFIFOL(fd,4)=sd->status.base_level;
		break;
	case SP_JOBLEVEL:
		WFIFOL(fd,4)=sd->status.job_level;
		break;
	case SP_KARMA: // Adding this back, I wonder if the client intercepts this - [Lance]
		WFIFOL(fd,4)=sd->status.karma;
		break;
	case SP_MANNER:
		WFIFOL(fd,4)=sd->status.manner;
		break;
	case SP_STATUSPOINT:
		WFIFOL(fd,4)=sd->status.status_point;
		break;
	case SP_SKILLPOINT:
		WFIFOL(fd,4)=sd->status.skill_point;
		break;
	case SP_HIT:
		WFIFOL(fd,4)=sd->battle_status.hit;
		break;
	case SP_FLEE1:
		WFIFOL(fd,4)=sd->battle_status.flee;
		break;
	case SP_FLEE2:
		WFIFOL(fd,4)=sd->battle_status.flee2/10;
		break;
	case SP_MAXHP:
		WFIFOL(fd,4)=sd->battle_status.max_hp;
		break;
	case SP_MAXSP:
		WFIFOL(fd,4)=sd->battle_status.max_sp;
		break;
	case SP_HP:
		WFIFOL(fd,4)=sd->battle_status.hp;
		if( battle_config.disp_hpmeter )
			clif_hpmeter(sd);
		if( !battle_config.party_hp_mode && sd->status.party_id )
			clif_party_hp(sd);
		if( sd->bg_id )
			clif_bg_hp(sd);
		break;
	case SP_SP:
		WFIFOL(fd,4)=sd->battle_status.sp;
		break;
	case SP_ASPD:
		WFIFOL(fd,4)=sd->battle_status.amotion;
		break;
	case SP_ATK1:
		if( battle_config.renewal_system_enable)
			WFIFOL(fd,4)=sd->battle_status.batk;
		else
			WFIFOL(fd,4)=sd->battle_status.batk +sd->battle_status.rhw.atk +sd->battle_status.lhw.atk;
		break;
	case SP_DEF1:
		if( battle_config.renewal_system_enable )
			WFIFOL(fd,4)=sd->battle_status.def2;
		else
			WFIFOL(fd,4)=sd->battle_status.def;
		break;
	case SP_MDEF1:
		if( battle_config.renewal_system_enable )
			WFIFOL(fd,4)=sd->battle_status.mdef2;
		else
			WFIFOL(fd,4)=sd->battle_status.mdef;
		break;
	case SP_ATK2:
		if( battle_config.renewal_system_enable )
			WFIFOL(fd,4)=sd->battle_status.rhw.atk + sd->battle_status.lhw.atk + sd->battle_status.rhw.atk2 + sd->battle_status.lhw.atk2;
		else
			WFIFOL(fd,4)=sd->battle_status.rhw.atk2 + sd->battle_status.lhw.atk2;
		break;
	case SP_DEF2:
		if( battle_config.renewal_system_enable )
			WFIFOL(fd,4)=sd->battle_status.def;
		else
			WFIFOL(fd,4)=sd->battle_status.def2;
		break;
	case SP_MDEF2:
		if( battle_config.renewal_system_enable )
			WFIFOL(fd,4)= sd->battle_status.mdef;
		else
		{
			//negative check (in case you have something like Berserk active)
			len = sd->battle_status.mdef2 - (sd->battle_status.vit>>1);
			if( len < 0 ) len = 0;
			WFIFOL(fd,4)= len;
			len = 8;
		}
		break;
	case SP_CRITICAL:
		WFIFOL(fd,4)=sd->battle_status.cri/10;
		break;
	case SP_MATK1:
		if( battle_config.renewal_system_enable )
			WFIFOL(fd,4)=sd->battle_status.matk_min;
		else
			WFIFOL(fd,4)=sd->battle_status.matk_max;
		break;
	case SP_MATK2:
		if( battle_config.renewal_system_enable )
			WFIFOL(fd,4)=sd->battle_status.matk;
		else
			WFIFOL(fd,4)=sd->battle_status.matk_min;
		break;


	case SP_ZENY:
		WFIFOW(fd,0)=0xb1;
		WFIFOL(fd,4)=sd->status.zeny;
		break;
	case SP_BASEEXP:
		WFIFOW(fd,0)=0xb1;
		WFIFOL(fd,4)=sd->status.base_exp;
		break;
	case SP_JOBEXP:
		WFIFOW(fd,0)=0xb1;
		WFIFOL(fd,4)=sd->status.job_exp;
		break;
	case SP_NEXTBASEEXP:
		WFIFOW(fd,0)=0xb1;
		WFIFOL(fd,4)=pc_nextbaseexp(sd);
		break;
	case SP_NEXTJOBEXP:
		WFIFOW(fd,0)=0xb1;
		WFIFOL(fd,4)=pc_nextjobexp(sd);
		break;

		// 00be �I��
	case SP_USTR:
	case SP_UAGI:
	case SP_UVIT:
	case SP_UINT:
	case SP_UDEX:
	case SP_ULUK:
		WFIFOW(fd,0)=0xbe;
		WFIFOB(fd,4)=pc_need_status_point(sd,type-SP_USTR+SP_STR,1);
		len=5;
		break;

		// 013a �I��
	case SP_ATTACKRANGE:
		WFIFOW(fd,0)=0x13a;
		WFIFOW(fd,2)=sd->battle_status.rhw.range;
		len=4;
		break;

		// 0141 �I��
	case SP_STR:
		WFIFOW(fd,0)=0x141;
		WFIFOL(fd,2)=type;
		WFIFOL(fd,6)=sd->status.str;
		WFIFOL(fd,10)=sd->battle_status.str - sd->status.str;
		len=14;
		break;
	case SP_AGI:
		WFIFOW(fd,0)=0x141;
		WFIFOL(fd,2)=type;
		WFIFOL(fd,6)=sd->status.agi;
		WFIFOL(fd,10)=sd->battle_status.agi - sd->status.agi;
		len=14;
		break;
	case SP_VIT:
		WFIFOW(fd,0)=0x141;
		WFIFOL(fd,2)=type;
		WFIFOL(fd,6)=sd->status.vit;
		WFIFOL(fd,10)=sd->battle_status.vit - sd->status.vit;
		len=14;
		break;
	case SP_INT:
		WFIFOW(fd,0)=0x141;
		WFIFOL(fd,2)=type;
		WFIFOL(fd,6)=sd->status.int_;
		WFIFOL(fd,10)=sd->battle_status.int_ - sd->status.int_;
		len=14;
		break;
	case SP_DEX:
		WFIFOW(fd,0)=0x141;
		WFIFOL(fd,2)=type;
		WFIFOL(fd,6)=sd->status.dex;
		WFIFOL(fd,10)=sd->battle_status.dex - sd->status.dex;
		len=14;
		break;
	case SP_LUK:
		WFIFOW(fd,0)=0x141;
		WFIFOL(fd,2)=type;
		WFIFOL(fd,6)=sd->status.luk;
		WFIFOL(fd,10)=sd->battle_status.luk - sd->status.luk;
		len=14;
		break;

	case SP_CARTINFO:
		WFIFOW(fd,0)=0x121;
		WFIFOW(fd,2)=sd->cart_num;
		WFIFOW(fd,4)=MAX_CART;
		WFIFOL(fd,6)=sd->cart_weight;
		WFIFOL(fd,10)=battle_config.max_cart_weight + (pc_checkskill(sd,GN_REMODELING_CART)*5000);
		len=14;
		break;

	default:
		ShowError("clif_updatestatus : unrecognized type %d\n",type);
		return 1;
	}
	WFIFOSET(fd,len);

	return 0;
}

int clif_changestatus(struct block_list *bl,int type,int val)
{
	unsigned char buf[12];
	struct map_session_data *sd = NULL;

	nullpo_ret(bl);

	if(bl->type == BL_PC)
		sd = (struct map_session_data *)bl;

	if(sd){
		WBUFW(buf,0)=0x1ab;
		WBUFL(buf,2)=bl->id;
		WBUFW(buf,6)=type;
		switch(type){
		case SP_MANNER:
			WBUFL(buf,8)=val;
			break;
		default:
			ShowError("clif_changestatus : unrecognized type %d.\n",type);
			return 1;
		}
		clif_send(buf,packet_len(0x1ab),bl,AREA_WOS);
	}
	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
void clif_changelook(struct block_list *bl,int type,int val)
{
	unsigned char buf[16];
	struct map_session_data* sd = NULL;
	struct status_change* sc;
	struct view_data* vd;
	enum send_target target = AREA;
	nullpo_retv(bl);

	sd = BL_CAST(BL_PC, bl);
	sc = status_get_sc(bl);
	vd = status_get_viewdata(bl);
	//nullpo_ret(vd);
	if( vd ) //temp hack to let Warp Portal change appearance
	switch(type)
	{
		case LOOK_WEAPON:
			if (sd)
			{
				clif_get_weapon_view(sd, &vd->weapon, &vd->shield);
				val = vd->weapon;
			}
			else vd->weapon = val;
		break;
		case LOOK_SHIELD:
			if (sd)
			{
				clif_get_weapon_view(sd, &vd->weapon, &vd->shield);
				val = vd->shield;
			}
			else vd->shield = val;
		break;
		case LOOK_BASE:
			vd->class_ = val;
			if (vd->class_ == JOB_WEDDING || vd->class_ == JOB_XMAS || vd->class_ == JOB_SUMMER)
				vd->weapon = vd->shield = 0;
			if (vd->cloth_color && (
				(vd->class_ == JOB_WEDDING && battle_config.wedding_ignorepalette) ||
				(vd->class_ == JOB_XMAS && battle_config.xmas_ignorepalette) ||
				(vd->class_ == JOB_SUMMER && battle_config.summer_ignorepalette)
			))
				clif_changelook(bl,LOOK_CLOTHES_COLOR,0);
		break;
		case LOOK_HAIR:
			vd->hair_style = val;
		break;
		case LOOK_HEAD_BOTTOM:
			vd->head_bottom = val;
		break;
		case LOOK_HEAD_TOP:
			vd->head_top = val;
		break;	
		case LOOK_HEAD_MID:
			vd->head_mid = val;
		break;
		case LOOK_HAIR_COLOR:
			vd->hair_color = val;
		break;
		case LOOK_CLOTHES_COLOR:
			if (val && (
				(vd->class_ == JOB_WEDDING && battle_config.wedding_ignorepalette) ||
				(vd->class_ == JOB_XMAS && battle_config.xmas_ignorepalette) ||
				(vd->class_ == JOB_SUMMER && battle_config.summer_ignorepalette)
			))
				val = 0;
			vd->cloth_color = val;
		break;
		case LOOK_SHOES:
#if PACKETVER > 3
			if (sd) {
				int n;
				if((n = sd->equip_index[2]) >= 0 && sd->inventory_data[n]) {
					if(sd->inventory_data[n]->view_id > 0)
						val = sd->inventory_data[n]->view_id;
					else
						val = sd->status.inventory[n].nameid;
					}
				val = 0;
			}
#endif
			//Shoes? No packet uses this....
		break;
		case LOOK_BODY:
		case LOOK_FLOOR:
			// unknown purpose
		break;
		case LOOK_ROBE:
#if PACKETVER < 20110111
			return;
#else
			vd->robe = val;
#endif
		break;
	}

	// prevent leaking the presence of GM-hidden objects
	if( sc && sc->option&OPTION_INVISIBLE )
		target = SELF;

#if PACKETVER < 4
	WBUFW(buf,0)=0xc3;
	WBUFL(buf,2)=bl->id;
	WBUFB(buf,6)=type;
	WBUFB(buf,7)=val;
	clif_send(buf,packet_len(0xc3),bl,target);
#else
	WBUFW(buf,0)=0x1d7;
	WBUFL(buf,2)=bl->id;
	if(type == LOOK_WEAPON || type == LOOK_SHIELD) {
		WBUFB(buf,6)=LOOK_WEAPON;
		WBUFW(buf,7)=vd->weapon;
		WBUFW(buf,9)=vd->shield;
	} else {
		WBUFB(buf,6)=type;
		WBUFL(buf,7)=val;
	}
	clif_send(buf,packet_len(0x1d7),bl,target);
#endif
}

//Sends a change-base-look packet required for traps as they are triggered.
void clif_changetraplook(struct block_list *bl,int val)
{
	unsigned char buf[32];
#if PACKETVER < 4
	WBUFW(buf,0)=0xc3;
	WBUFL(buf,2)=bl->id;
	WBUFB(buf,6)=LOOK_BASE;
	WBUFB(buf,7)=val;
	clif_send(buf,packet_len(0xc3),bl,AREA);
#else
	WBUFW(buf,0)=0x1d7;
	WBUFL(buf,2)=bl->id;
	WBUFB(buf,6)=LOOK_BASE;
	WBUFW(buf,7)=val;
	WBUFW(buf,9)=0;
	clif_send(buf,packet_len(0x1d7),bl,AREA);
#endif

	
}
//For the stupid cloth-dye bug. Resends the given view data to the area specified by bl.
void clif_refreshlook(struct block_list *bl,int id,int type,int val,enum send_target target)
{
	unsigned char buf[32];
#if PACKETVER < 4
	WBUFW(buf,0)=0xc3;
	WBUFL(buf,2)=id;
	WBUFB(buf,6)=type;
	WBUFB(buf,7)=val;
	clif_send(buf,packet_len(0xc3),bl,target);
#else
	WBUFW(buf,0)=0x1d7;
	WBUFL(buf,2)=id;
	WBUFB(buf,6)=type;
	WBUFW(buf,7)=val;
	WBUFW(buf,9)=0;
	clif_send(buf,packet_len(0x1d7),bl,target);
#endif
	return;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_initialstatus(struct map_session_data *sd)
{
	int fd;
	unsigned char *buf;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0xbd));
	buf=WFIFOP(fd,0);

	WBUFW(buf,0)=0xbd;
	WBUFW(buf,2)=min(sd->status.status_point, INT16_MAX);
	WBUFB(buf,4)=min(sd->status.str, UINT8_MAX);
	WBUFB(buf,5)=pc_need_status_point(sd,SP_STR,1);
	WBUFB(buf,6)=min(sd->status.agi, UINT8_MAX);
	WBUFB(buf,7)=pc_need_status_point(sd,SP_AGI,1);
	WBUFB(buf,8)=min(sd->status.vit, UINT8_MAX);
	WBUFB(buf,9)=pc_need_status_point(sd,SP_VIT,1);
	WBUFB(buf,10)=min(sd->status.int_, UINT8_MAX);
	WBUFB(buf,11)=pc_need_status_point(sd,SP_INT,1);
	WBUFB(buf,12)=min(sd->status.dex, UINT8_MAX);
	WBUFB(buf,13)=pc_need_status_point(sd,SP_DEX,1);
	WBUFB(buf,14)=min(sd->status.luk, UINT8_MAX);
	WBUFB(buf,15)=pc_need_status_point(sd,SP_LUK,1);

	if( battle_config.renewal_system_enable )
	{
		WBUFW(buf,16) = sd->battle_status.batk;
		WBUFW(buf,18) = sd->battle_status.rhw.atk + sd->battle_status.rhw.atk2 + sd->battle_status.lhw.atk + sd->battle_status.lhw.atk2;
		WBUFW(buf,20) = sd->battle_status.matk_min;
		WBUFW(buf,22) = sd->battle_status.matk;
		WBUFW(buf,24) = sd->battle_status.def2;
		WBUFW(buf,26) = sd->battle_status.def;
		WBUFW(buf,28) = sd->battle_status.mdef2;
		WBUFW(buf,30) = sd->battle_status.mdef;
	}
	else
	{
		int mdef2;
		WBUFW(buf,16) = sd->battle_status.batk + sd->battle_status.rhw.atk + sd->battle_status.lhw.atk;
		WBUFW(buf,18) = sd->battle_status.rhw.atk2 + sd->battle_status.lhw.atk2; //atk bonus
		WBUFW(buf,20) = sd->battle_status.matk_max;
		WBUFW(buf,22) = sd->battle_status.matk_min;
		WBUFW(buf,24) = sd->battle_status.def;
		WBUFW(buf,26) = sd->battle_status.def2;
		WBUFW(buf,28) = sd->battle_status.mdef; // mdef
		if( (mdef2 = sd->battle_status.mdef2 - (sd->battle_status.vit>>1)) < 0 )
			mdef2 = 0;
		WBUFW(buf,30) = mdef2;
	}
	WBUFW(buf,32) = sd->battle_status.hit;
	WBUFW(buf,34) = sd->battle_status.flee;
	WBUFW(buf,36) = sd->battle_status.flee2/10;
	WBUFW(buf,38) = sd->battle_status.cri/10;
	WBUFW(buf,40) = sd->battle_status.amotion; // aspd
	WBUFW(buf,42) = sd->status.manner;  // FIXME: This is 'plusASPD', but what is it supposed to be?

	WFIFOSET(fd,packet_len(0xbd));

	clif_updatestatus(sd,SP_STR);
	clif_updatestatus(sd,SP_AGI);
	clif_updatestatus(sd,SP_VIT);
	clif_updatestatus(sd,SP_INT);
	clif_updatestatus(sd,SP_DEX);
	clif_updatestatus(sd,SP_LUK);

	clif_updatestatus(sd,SP_ATTACKRANGE);
	clif_updatestatus(sd,SP_ASPD);

	return 0;
}

/*==========================================
 *���
 *------------------------------------------*/
int clif_arrowequip(struct map_session_data *sd,int val)
{
	int fd;

	nullpo_ret(sd);

	pc_stop_attack(sd); // [Valaris]

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len(0x013c));
	WFIFOW(fd,0)=0x013c;
	WFIFOW(fd,2)=val+2;//��̃A�C�e��ID
	WFIFOSET(fd,packet_len(0x013c));

	return 0;
}

/// Ammunition action message.
/// type=0 : MsgStringTable[242]="Please equip the proper ammunition first."
/// type=1 : MsgStringTable[243]="You can't Attack or use Skills because your Weight Limit has been exceeded."
/// type=2 : MsgStringTable[244]="You can't use Skills because Weight Limit has been exceeded."
/// type=3 : assassin, baby_assassin, assassin_cross => MsgStringTable[1040]="You have equipped throwing daggers."
///          gunslinger => MsgStringTable[1175]="Bullets have been equipped."
///          NOT ninja => MsgStringTable[245]="Ammunition has been equipped."
int clif_arrow_fail(struct map_session_data *sd,int type)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len(0x013b));
	WFIFOW(fd,0)=0x013b;
	WFIFOW(fd,2)=type;
	WFIFOSET(fd,packet_len(0x013b));

	return 0;
}

/*==========================================
 * �쐬�\ ��X�g���M
 *------------------------------------------*/
int clif_arrow_create_list(struct map_session_data *sd)
{
	int i, c, j;
	int fd;

	nullpo_ret(sd);

	fd = sd->fd;
	WFIFOHEAD(fd, MAX_SKILL_ARROW_DB*2+4);
	WFIFOW(fd,0) = 0x1ad;

	for (i = 0, c = 0; i < MAX_SKILL_ARROW_DB; i++) {
		if (skill_arrow_db[i].nameid > 0 &&
			(j = pc_search_inventory(sd, skill_arrow_db[i].nameid)) >= 0 &&
			!sd->status.inventory[j].equip && sd->status.inventory[j].identify)
		{
			if ((j = itemdb_viewid(skill_arrow_db[i].nameid)) > 0)
				WFIFOW(fd,c*2+4) = j;
			else
				WFIFOW(fd,c*2+4) = skill_arrow_db[i].nameid;
			c++;
		}
	}
	WFIFOW(fd,2) = c*2+4;
	WFIFOSET(fd, WFIFOW(fd,2));
	if (c > 0) {
		sd->menuskill_id = AC_MAKINGARROW;
		sd->menuskill_val = c;
	}

	return 0;
}

/*==========================================
 * Guillotine Cross Poisons List
 *------------------------------------------*/
int clif_poison_list(struct map_session_data *sd, int skill_lv)
{
	int i, c;
	int fd;

	nullpo_ret(sd);

	fd = sd->fd;
	WFIFOHEAD(fd, 8 * 8 + 8);
	WFIFOW(fd,0) = 0x1ad; // This is the official packet. [pakpil]

	for( i = 0, c = 0; i < MAX_INVENTORY; i ++ )
	{
		if( itemdb_is_poison(sd->status.inventory[i].nameid) )
		{ 
			WFIFOW(fd, c * 2 + 4) = sd->status.inventory[i].nameid;
			c ++;
		}
	}
	if( c > 0 )
	{
		sd->menuskill_id = GC_POISONINGWEAPON;
		sd->menuskill_val = c;
		sd->menuskill_itemused = skill_lv;
		WFIFOW(fd,2) = c * 2 + 4;
		WFIFOSET(fd, WFIFOW(fd, 2));
	}
	else
	{
		clif_skill_fail(sd,GC_POISONINGWEAPON,0x2b,0);
		return 0;
	}

	return 1;
}

/*==========================================
 * Magic Decoy Material List
 *------------------------------------------*/
int clif_magicdecoy_list(struct map_session_data *sd, int skill_lv, short x, short y)
{
	int i, c;
	int fd;

	nullpo_ret(sd);

	fd = sd->fd;
	WFIFOHEAD(fd, 8 * 8 + 8);
	WFIFOW(fd,0) = 0x1ad; // This is the official packet. [pakpil]

	for( i = 0, c = 0; i < MAX_INVENTORY; i ++ )
	{
		if( itemdb_is_element(sd->status.inventory[i].nameid) )
		{ 
			WFIFOW(fd, c * 2 + 4) = sd->status.inventory[i].nameid;
			c ++;
		}
	}
	if( c > 0 )
	{
		sd->menuskill_id = NC_MAGICDECOY;
		sd->menuskill_val = skill_lv;
		sd->menuskill_itemused = (x<<16)|y;
		WFIFOW(fd,2) = c * 2 + 4;
		WFIFOSET(fd, WFIFOW(fd, 2));
	}
	else
	{
		clif_skill_fail(sd,NC_MAGICDECOY,0,0);
		return 0;
	}

	return 1;
}

/*==========================================
 * Spellbook list [LimitLine]
 *------------------------------------------*/
int clif_spellbook_list(struct map_session_data *sd)
{
	int i, c;
	int fd;

	nullpo_ret(sd);

	fd = sd->fd;
	WFIFOHEAD(fd, 8 * 8 + 8);
	WFIFOW(fd,0) = 0x1ad;

	for( i = 0, c = 0; i < MAX_INVENTORY; i ++ )
	{
		if( itemdb_is_spellbook(sd->status.inventory[i].nameid) )
		{ 
			WFIFOW(fd, c * 2 + 4) = sd->status.inventory[i].nameid;
			c ++;
		}
	}
	
	if( c > 0 )
	{
		WFIFOW(fd,2) = c * 2 + 4;
		WFIFOSET(fd, WFIFOW(fd, 2));
		sd->menuskill_id = WL_READING_SB;
		sd->menuskill_val = c;
	}
	else
		status_change_end(&sd->bl,SC_STOP,-1);

	return 1;
}

/*===========================================
 * Skill list for Auto Shadow Spell
 *------------------------------------------*/
int clif_skill_select_request(struct map_session_data *sd)
{
	int fd, i, c;
	nullpo_ret(sd);
	fd = sd->fd;
	if( !fd ) return 0;

	if( sd->menuskill_id == SC_AUTOSHADOWSPELL )
		return 0;

	WFIFOHEAD(fd, 2 * 6 + 4);
	WFIFOW(fd,0) = 0x442;
	for( i = 0, c = 0; i < MAX_SKILL; i++ )
		if( sd->status.skill[i].flag == 13 && sd->status.skill[i].id > 0 && sd->status.skill[i].id < GS_GLITTERING && skill_get_type(sd->status.skill[i].id) == BF_MAGIC )
		{ // Can't auto cast both Extended class and 3rd class skills.
			WFIFOW(fd,8+c*2) = sd->status.skill[i].id;
			c++;
		}

	if( c > 0 )
	{
		WFIFOW(fd,2) = 8 + c * 2;
		WFIFOL(fd,4) = c;
		WFIFOSET(fd,WFIFOW(fd,2));
		sd->menuskill_id = SC_AUTOSHADOWSPELL;
		sd->menuskill_val = c;
	}
	else
	{
		status_change_end(&sd->bl,SC_STOP,-1);
		clif_skill_fail(sd,SC_AUTOSHADOWSPELL,0x15,0);
	}

	return 1;
}

/*===========================================
 * Skill list for Four Elemental Analysis
 * and Change Material skills.
 *------------------------------------------*/
int clif_skill_itemlistwindow( struct map_session_data *sd, int skill_id, int skill_lv )
{
#if PACKETVER >= 20090922
	int fd;

	nullpo_ret(sd);
	
	sd->menuskill_id = skill_id; // To prevent hacking.
	sd->menuskill_val = skill_lv;

	if( skill_id == GN_CHANGEMATERIAL )
		skill_lv = 0; // Changematerial

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0x7e3));
	WFIFOW(fd,0) = 0x7e3;
	WFIFOL(fd,2) = skill_lv;
	WFIFOL(fd,4) = 0;
	WFIFOSET(fd,packet_len(0x7e3));

#endif

	return 1;

}

/*==========================================
 *
 *------------------------------------------*/
int clif_statusupack(struct map_session_data *sd,int type,int ok,int val)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0xbc));
	WFIFOW(fd,0)=0xbc;
	WFIFOW(fd,2)=type;
	WFIFOB(fd,4)=ok;
	WFIFOB(fd,5)=cap_value(val,0,UINT8_MAX);
	WFIFOSET(fd,packet_len(0xbc));

	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_equipitemack(struct map_session_data *sd,int n,int pos,int ok)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0xaa));
	WFIFOW(fd,0)=0xaa;
	WFIFOW(fd,2)=n+2;
	WFIFOW(fd,4)=pos;
#if PACKETVER < 20100629
	WFIFOB(fd,6)=ok;
#else
	if (ok && sd->inventory_data[n]->equip&EQP_VISIBLE)
		WFIFOW(fd,6)=sd->inventory_data[n]->look;
	else
		WFIFOW(fd,6)=0;
	WFIFOB(fd,8)=ok;
#endif
	WFIFOSET(fd,packet_len(0xaa));

	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_unequipitemack(struct map_session_data *sd,int n,int pos,int ok)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0xac));
	WFIFOW(fd,0)=0xac;
	WFIFOW(fd,2)=n+2;
	WFIFOW(fd,4)=pos;
	WFIFOB(fd,6)=ok;
	WFIFOSET(fd,packet_len(0xac));

	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_misceffect(struct block_list* bl,int type)
{
	unsigned char buf[32];

	nullpo_ret(bl);

	WBUFW(buf,0) = 0x19b;
	WBUFL(buf,2) = bl->id;
	WBUFL(buf,6) = type;

	clif_send(buf,packet_len(0x19b),bl,AREA);

	return 0;
}

/*==========================================
 * �\���I�v�V�����ύX
 *------------------------------------------*/
int clif_changeoption(struct block_list* bl)
{
	unsigned char buf[32];
	struct status_change *sc;
	struct map_session_data* sd;

	nullpo_ret(bl);
	sc = status_get_sc(bl);
	if (!sc) return 0; //How can an option change if there's no sc?
	sd = BL_CAST(BL_PC, bl);
	
#if PACKETVER >= 7
	WBUFW(buf,0) = 0x229;
	WBUFL(buf,2) = bl->id;
	WBUFW(buf,6) = sc->opt1;
	WBUFW(buf,8) = sc->opt2;
	WBUFL(buf,10) = sc->option;
	WBUFB(buf,14) = (sd)? sd->status.karma : 0;
	if(disguised(bl)) {
		clif_send(buf,packet_len(0x229),bl,AREA_WOS);
		WBUFL(buf,2) = -bl->id;
		clif_send(buf,packet_len(0x229),bl,SELF);
		WBUFL(buf,2) = bl->id;
		WBUFL(buf,10) = OPTION_INVISIBLE;
		clif_send(buf,packet_len(0x229),bl,SELF);
	} else
		clif_send(buf,packet_len(0x229),bl,AREA);
#else
	WBUFW(buf,0) = 0x119;
	WBUFL(buf,2) = bl->id;
	WBUFW(buf,6) = sc->opt1;
	WBUFW(buf,8) = sc->opt2;
	WBUFW(buf,10) = sc->option;
	WBUFB(buf,12) = (sd)? sd->status.karma : 0;
	if(disguised(bl)) {
		clif_send(buf,packet_len(0x119),bl,AREA_WOS);
		WBUFL(buf,2) = -bl->id;
		clif_send(buf,packet_len(0x119),bl,SELF);
		WBUFL(buf,2) = bl->id;
		WBUFW(buf,10) = OPTION_INVISIBLE;
		clif_send(buf,packet_len(0x119),bl,SELF);
	} else
		clif_send(buf,packet_len(0x119),bl,AREA);
#endif

	return 0;
}

int clif_changeoption2(struct block_list* bl)
{
	unsigned char buf[20];
	struct status_change *sc;
	
	sc = status_get_sc(bl);
	if (!sc) return 0; //How can an option change if there's no sc?

	WBUFW(buf,0) = 0x28a;
	WBUFL(buf,2) = bl->id;
	WBUFL(buf,6) = sc->option;
	WBUFL(buf,10) = clif_setlevel(bl);
	WBUFL(buf,14) = sc->opt3;
	if(disguised(bl)) {
		clif_send(buf,packet_len(0x28a),bl,AREA_WOS);
		WBUFL(buf,2) = -bl->id;
		clif_send(buf,packet_len(0x28a),bl,SELF);
		WBUFL(buf,2) = bl->id;
		WBUFL(buf,6) = OPTION_INVISIBLE;
		clif_send(buf,packet_len(0x28a),bl,SELF);
	} else
		clif_send(buf,packet_len(0x28a),bl,AREA);
	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_useitemack(struct map_session_data *sd,int index,int amount,int ok)
{
	nullpo_ret(sd);

	if(!ok) {
		int fd=sd->fd;
  		WFIFOHEAD(fd,packet_len(0xa8));
		WFIFOW(fd,0)=0xa8;
		WFIFOW(fd,2)=index+2;
		WFIFOW(fd,4)=amount;
		WFIFOB(fd,6)=ok;
		WFIFOSET(fd,packet_len(0xa8));
	}
	else {
#if PACKETVER < 3
		int fd=sd->fd;
		WFIFOHEAD(fd,packet_len(0xa8));
		WFIFOW(fd,0)=0xa8;
		WFIFOW(fd,2)=index+2;
		WFIFOW(fd,4)=amount;
		WFIFOB(fd,6)=ok;
		WFIFOSET(fd,packet_len(0xa8));
#else
		unsigned char buf[32];

		WBUFW(buf,0)=0x1c8;
		WBUFW(buf,2)=index+2;
		if(sd->inventory_data[index] && sd->inventory_data[index]->view_id > 0)
			WBUFW(buf,4)=sd->inventory_data[index]->view_id;
		else
			WBUFW(buf,4)=sd->status.inventory[index].nameid;
		WBUFL(buf,6)=sd->bl.id;
		WBUFW(buf,10)=amount;
		WBUFB(buf,12)=ok;
		clif_send(buf,packet_len(0x1c8),&sd->bl,AREA);
#endif
	}

	return 0;
}

/// Inform client whether chatroom creation was successful or not
/// R 00d6 <flag>.B
/// flag:
///  0 = Room has been successfully created (opens chat room)
///  1 = Room limit exceeded
///  2 = Same room already exists
void clif_createchat(struct map_session_data* sd, int flag)
{
	int fd;

	nullpo_retv(sd);

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0xd6));
	WFIFOW(fd,0) = 0xd6;
	WFIFOB(fd,2) = flag;
	WFIFOSET(fd,packet_len(0xd6));
}

/*==========================================
 * Display a chat above the owner
 * R 00d7 <len>.w <owner ID>.l <chat ID>.l <limit>.w <users>.w <type>.B <title>.?B
 *------------------------------------------*/
int clif_dispchat(struct chat_data* cd, int fd)
{
	unsigned char buf[128];
	uint8 type;

	if( cd == NULL || cd->owner == NULL )
		return 1;

	// type - 0: private, 1: public, 2: npc, 3: non-clickable
	type = (cd->owner->type == BL_PC ) ? (cd->pub) ? 1 : 0
	     : (cd->owner->type == BL_NPC) ? (cd->limit) ? 2 : 3
	     : 1;

	WBUFW(buf, 0) = 0xd7;
	WBUFW(buf, 2) = 17 + strlen(cd->title);
	WBUFL(buf, 4) = cd->owner->id;
	WBUFL(buf, 8) = cd->bl.id;
	WBUFW(buf,12) = cd->limit;
	WBUFW(buf,14) = cd->users;
	WBUFB(buf,16) = type;
	strncpy((char*)WBUFP(buf,17), cd->title, strlen(cd->title)); // not zero-terminated

	if( fd ) {
		WFIFOHEAD(fd,WBUFW(buf,2));
		memcpy(WFIFOP(fd,0),buf,WBUFW(buf,2));
		WFIFOSET(fd,WBUFW(buf,2));
	} else {
		clif_send(buf,WBUFW(buf,2),cd->owner,AREA_WOSC);
	}

	return 0;
}

/*==========================================
 * Chatroom properties adjustment
 * R 00df <len>.w <owner ID>.l <chat ID>.l <limit>.w <users>.w <pub>.B <title>.?B
 *------------------------------------------*/
int clif_changechatstatus(struct chat_data* cd)
{
	unsigned char buf[128];

	if( cd == NULL || cd->usersd[0] == NULL )
		return 1;

	WBUFW(buf, 0) = 0xdf;
	WBUFW(buf, 2) = 17 + strlen(cd->title);
	WBUFL(buf, 4) = cd->usersd[0]->bl.id;
	WBUFL(buf, 8) = cd->bl.id;
	WBUFW(buf,12) = cd->limit;
	WBUFW(buf,14) = cd->users;
	WBUFB(buf,16) = cd->pub;
	strncpy((char*)WBUFP(buf,17), cd->title, strlen(cd->title)); // not zero-terminated

	clif_send(buf,WBUFW(buf,2),&cd->usersd[0]->bl,CHAT);

	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_clearchat(struct chat_data *cd,int fd)
{
	unsigned char buf[32];

	nullpo_ret(cd);

	WBUFW(buf,0) = 0xd8;
	WBUFL(buf,2) = cd->bl.id;
	if( fd ) {
		WFIFOHEAD(fd,packet_len(0xd8));
		memcpy(WFIFOP(fd,0),buf,packet_len(0xd8));
		WFIFOSET(fd,packet_len(0xd8));
	} else {
		clif_send(buf,packet_len(0xd8),cd->owner,AREA_WOSC);
	}

	return 0;
}

/// Displays message (mostly) regarding join chat
/// failures
/// R 0x00da <flag>.B
/// flag:
///  0 = The room is already full.
///  1 = Incorrect password, please try again.
///  2 = You have been kicked out of the room.
///  4 = You don't have enough money.
///  5 = You are not the required level.
///  6 = Too high level for this job.
///  7 = Not the suitable job for this type of work.
int clif_joinchatfail(struct map_session_data *sd,int flag)
{
	int fd;

	nullpo_ret(sd);

	fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0xda));
	WFIFOW(fd,0) = 0xda;
	WFIFOB(fd,2) = flag;
	WFIFOSET(fd,packet_len(0xda));

	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_joinchatok(struct map_session_data *sd,struct chat_data* cd)
{
	int fd;
	int i;

	nullpo_ret(sd);
	nullpo_ret(cd);

	fd = sd->fd;
	if (!session_isActive(fd))
		return 0;
	WFIFOHEAD(fd, 8 + (28*cd->users));
	WFIFOW(fd, 0) = 0xdb;
	WFIFOW(fd, 2) = 8 + (28*cd->users);
	WFIFOL(fd, 4) = cd->bl.id;
	for (i = 0; i < cd->users; i++) {
		WFIFOL(fd, 8+i*28) = (i != 0 || cd->owner->type == BL_NPC);
		memcpy(WFIFOP(fd, 8+i*28+4), cd->usersd[i]->status.name, NAME_LENGTH);
	}
	WFIFOSET(fd, WFIFOW(fd, 2));

	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_addchat(struct chat_data* cd,struct map_session_data *sd)
{
	unsigned char buf[32];

	nullpo_ret(sd);
	nullpo_ret(cd);

	WBUFW(buf, 0) = 0x0dc;
	WBUFW(buf, 2) = cd->users;
	memcpy(WBUFP(buf, 4),sd->status.name,NAME_LENGTH);
	clif_send(buf,packet_len(0xdc),&sd->bl,CHAT_WOS);

	return 0;
}

/*==========================================
 * Announce the new owner (ZC_ROLE_CHANGE)
 * R 00e1 <role>.L <nick>.24B
 *------------------------------------------*/
void clif_changechatowner(struct chat_data* cd, struct map_session_data* sd)
{
	unsigned char buf[64];

	nullpo_retv(sd);
	nullpo_retv(cd);

	WBUFW(buf, 0) = 0xe1;
	WBUFL(buf, 2) = 1;  // normal
	memcpy(WBUFP(buf,6),cd->usersd[0]->status.name,NAME_LENGTH);

	WBUFW(buf,30) = 0xe1;
	WBUFL(buf,32) = 0;  // owner (menu)
	memcpy(WBUFP(buf,36),sd->status.name,NAME_LENGTH);

	clif_send(buf,packet_len(0xe1)*2,&sd->bl,CHAT);
}

/*==========================================
 * Notify about user leaving the chatroom
 * R 00dd <index>.w <nick>.24B <flag>.B
 *------------------------------------------*/
void clif_leavechat(struct chat_data* cd, struct map_session_data* sd, bool flag)
{
	unsigned char buf[32];

	nullpo_retv(sd);
	nullpo_retv(cd);

	WBUFW(buf, 0) = 0xdd;
	WBUFW(buf, 2) = cd->users-1;
	memcpy(WBUFP(buf,4),sd->status.name,NAME_LENGTH);
	WBUFB(buf,28) = flag; // 0: left, 1: was kicked

	clif_send(buf,packet_len(0xdd),&sd->bl,CHAT);
}

/*==========================================
 * Opens a trade request window from char 'name'
 * R 00e5 <nick>.24B
 * R 01f4 <nick>.24B <charid>.L <baselvl>.W
 *------------------------------------------*/
void clif_traderequest(struct map_session_data* sd, const char* name)
{
	int fd = sd->fd;

#if PACKETVER < 6
	WFIFOHEAD(fd,packet_len(0xe5));
	WFIFOW(fd,0) = 0xe5;
	safestrncpy((char*)WFIFOP(fd,2), name, NAME_LENGTH);
	WFIFOSET(fd,packet_len(0xe5));
#else
	struct map_session_data* tsd = map_id2sd(sd->trade_partner);
	if( !tsd ) return;
	
	WFIFOHEAD(fd,packet_len(0x1f4));
	WFIFOW(fd,0) = 0x1f4;
	safestrncpy((char*)WFIFOP(fd,2), name, NAME_LENGTH);
	WFIFOL(fd,26) = tsd->status.char_id;
	WFIFOW(fd,30) = tsd->status.base_level;
	WFIFOSET(fd,packet_len(0x1f4));
#endif
}

/*==========================================
 * Reply to a trade-request.
 * R 00e7 <type>.B
 * R 01f5 <type>.B <charid>.L <baselvl>.W
 * Type:
 * 0: Char is too far
 * 1: Character does not exist
 * 2: Trade failed
 * 3: Accept
 * 4: Cancel
 *------------------------------------------*/
void clif_tradestart(struct map_session_data* sd, uint8 type)
{
	int fd = sd->fd;
	struct map_session_data* tsd = map_id2sd(sd->trade_partner);
	if( PACKETVER < 6 || !tsd ) {
		WFIFOHEAD(fd,packet_len(0xe7));
		WFIFOW(fd,0) = 0xe7;
		WFIFOB(fd,2) = type;
		WFIFOSET(fd,packet_len(0xe7));
	} else {
		WFIFOHEAD(fd,packet_len(0x1f5));
		WFIFOW(fd,0) = 0x1f5;
		WFIFOB(fd,2) = type;
		WFIFOL(fd,3) = tsd->status.char_id;
		WFIFOW(fd,7) = tsd->status.base_level;
		WFIFOSET(fd,packet_len(0x1f5));
	}
}

/*==========================================
 * ���������̃A�C�e���ǉ�
 *------------------------------------------*/
void clif_tradeadditem(struct map_session_data* sd, struct map_session_data* tsd, int index, int amount)
{
	int fd;
	unsigned char *buf;
#if PACKETVER < 20100223
	const int cmd = 0xe9;
#else
	const int cmd = 0x80f;
#endif
	nullpo_retv(sd);
	nullpo_retv(tsd);

	fd = tsd->fd;
	buf = WFIFOP(fd,0);
	WFIFOHEAD(fd,packet_len(cmd));
	WBUFW(buf,0) = cmd;
	if( index == 0 )
	{
#if PACKETVER < 20100223
		WBUFL(buf,2) = amount; //amount
		WBUFW(buf,6) = 0; // type id
#else
		WBUFW(buf,2) = 0;      // type id
		WBUFB(buf,4) = 0;      // item type
		WBUFL(buf,5) = amount; // amount
		buf = WBUFP(buf,1); //Advance 1B
#endif
		WBUFB(buf,8) = 0; //identify flag
		WBUFB(buf,9) = 0; // attribute
		WBUFB(buf,10)= 0; //refine
		WBUFW(buf,11)= 0; //card (4w)
		WBUFW(buf,13)= 0; //card (4w)
		WBUFW(buf,15)= 0; //card (4w)
		WBUFW(buf,17)= 0; //card (4w)
	}
	else
	{
		index -= 2; //index fix
#if PACKETVER < 20100223
		WBUFL(buf,2) = amount; //amount
		if(sd->inventory_data[index] && sd->inventory_data[index]->view_id > 0)
			WBUFW(buf,6) = sd->inventory_data[index]->view_id;
		else
			WBUFW(buf,6) = sd->status.inventory[index].nameid; // type id
#else
		if(sd->inventory_data[index] && sd->inventory_data[index]->view_id > 0)
			WBUFW(buf,2) = sd->inventory_data[index]->view_id;
		else
			WBUFW(buf,2) = sd->status.inventory[index].nameid;       // type id
		WBUFB(buf,4) = sd->inventory_data[index]->type;          // item type
		WBUFL(buf,5) = amount; // amount
		buf = WBUFP(buf,1); //Advance 1B
#endif
		WBUFB(buf,8) = sd->status.inventory[index].identify; //identify flag
		WBUFB(buf,9) = sd->status.inventory[index].attribute; // attribute
		WBUFB(buf,10)= sd->status.inventory[index].refine; //refine
		clif_addcards(WBUFP(buf, 11), &sd->status.inventory[index]);
	}
	WFIFOSET(fd,packet_len(cmd));
}

/*==========================================
 * �A�C�e���ǉ�����/���s
 *------------------------------------------*/
void clif_tradeitemok(struct map_session_data* sd, int index, int fail)
{
	int fd;
	nullpo_retv(sd);

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0xea));
	WFIFOW(fd,0) = 0xea;
	WFIFOW(fd,2) = index;
	WFIFOB(fd,4) = fail;
	WFIFOSET(fd,packet_len(0xea));
}

/*==========================================
 * ������ok����
 *------------------------------------------*/
void clif_tradedeal_lock(struct map_session_data* sd, int fail)
{
	int fd;
	nullpo_retv(sd);

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0xec));
	WFIFOW(fd,0) = 0xec;
	WFIFOB(fd,2) = fail; // 0=you 1=the other person
	WFIFOSET(fd,packet_len(0xec));
}

/*==========================================
 * ���������L�����Z������܂���
 *------------------------------------------*/
void clif_tradecancelled(struct map_session_data* sd)
{
	int fd;
	nullpo_retv(sd);

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0xee));
	WFIFOW(fd,0) = 0xee;
	WFIFOSET(fd,packet_len(0xee));
}

/*==========================================
 * ����������
 *------------------------------------------*/
void clif_tradecompleted(struct map_session_data* sd, int fail)
{
	int fd;
	nullpo_retv(sd);

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0xf0));
	WFIFOW(fd,0) = 0xf0;
	WFIFOB(fd,2) = fail;
	WFIFOSET(fd,packet_len(0xf0));
}

/*==========================================
 * �J�v���q�ɂ̃A�C�e�������X�V
 *------------------------------------------*/
void clif_updatestorageamount(struct map_session_data* sd, int amount)
{
	int fd;

	nullpo_retv(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0xf2));
	WFIFOW(fd,0) = 0xf2;  // update storage amount
	WFIFOW(fd,2) = amount;  //items
	WFIFOW(fd,4) = MAX_STORAGE; //items max
	WFIFOSET(fd,packet_len(0xf2));
}

void clif_updateextrastorageamount(struct map_session_data* sd, int amount)
{
	int fd;

	nullpo_retv(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0xf2));
	WFIFOW(fd,0) = 0xf2;  // update storage amount
	WFIFOW(fd,2) = amount;  //items
	WFIFOW(fd,4) = MAX_EXTRA_STORAGE; //items max
	WFIFOSET(fd,packet_len(0xf2));
}

/*==========================================
 * �J�v���q�ɂɃA�C�e����ǉ�����
 *------------------------------------------*/
void clif_storageitemadded(struct map_session_data* sd, struct item* i, int index, int amount)
{
	int view,fd;

	nullpo_retv(sd);
	nullpo_retv(i);
	fd=sd->fd;
	view = itemdb_viewid(i->nameid);

#if PACKETVER < 5
	WFIFOHEAD(fd,packet_len(0xf4));
	WFIFOW(fd, 0) = 0xf4; // Storage item added
	WFIFOW(fd, 2) = index+1; // index
	WFIFOL(fd, 4) = amount; // amount
	WFIFOW(fd, 8) = ( view > 0 ) ? view : i->nameid; // id
	WFIFOB(fd,10) = i->identify; //identify flag
	WFIFOB(fd,11) = i->attribute; // attribute
	WFIFOB(fd,12) = i->refine; //refine
	clif_addcards(WFIFOP(fd,13), i);
	WFIFOSET(fd,packet_len(0xf4));
#else
	WFIFOHEAD(fd,packet_len(0x1c4));
	WFIFOW(fd, 0) = 0x1c4; // Storage item added
	WFIFOW(fd, 2) = index+1; // index
	WFIFOL(fd, 4) = amount; // amount
	WFIFOW(fd, 8) = ( view > 0 ) ? view : i->nameid; // id
	WFIFOB(fd,10) = itemdb_type(i->nameid); //type
	WFIFOB(fd,11) = i->identify; //identify flag
	WFIFOB(fd,12) = i->attribute; // attribute
	WFIFOB(fd,13) = i->refine; //refine
	clif_addcards(WFIFOP(fd,14), i);
	WFIFOSET(fd,packet_len(0x1c4));
#endif
}

/*==========================================
 *
 *------------------------------------------*/
void clif_updateguildstorageamount(struct map_session_data* sd, int amount)
{
	int fd;

	nullpo_retv(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0xf2));
	WFIFOW(fd,0) = 0xf2;  // update storage amount
	WFIFOW(fd,2) = amount;  //items
	WFIFOW(fd,4) = MAX_GUILD_STORAGE; //items max
	WFIFOSET(fd,packet_len(0xf2));
}

/*==========================================
 * �J�v���q�ɂ���A�C�e������苎��
 *------------------------------------------*/
void clif_storageitemremoved(struct map_session_data* sd, int index, int amount)
{
	int fd;

	nullpo_retv(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0xf6));
	WFIFOW(fd,0)=0xf6; // Storage item removed
	WFIFOW(fd,2)=index+1;
	WFIFOL(fd,4)=amount;
	WFIFOSET(fd,packet_len(0xf6));
}

/*==========================================
 * �J�v���q�ɂ����
 *------------------------------------------*/
void clif_storageclose(struct map_session_data* sd)
{
	int fd;

	nullpo_retv(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0xf8));
	WFIFOW(fd,0) = 0xf8; // Storage Closed
	WFIFOSET(fd,packet_len(0xf8));
}

/*==========================================
 * Display Riding when someone under this
 * status change walk into your view range.
 *------------------------------------------*/
void clif_display_riding(struct block_list *dst, struct block_list *bl)
{
	unsigned char buf[32];

	nullpo_retv(bl);
	nullpo_retv(dst);

	if( battle_config.display_status_timers )
		WBUFW(buf,0) = 0x043f;
	else
		WBUFW(buf,0) = 0x0196;

	WBUFW(buf,2) = SI_ALL_RIDING;
	WBUFL(buf,4) = bl->id;
	WBUFB(buf,8) = 1;
	if( battle_config.display_status_timers )
	{
		WBUFL(buf,9) = 0;
		WBUFL(buf,13) = 0;
		WBUFL(buf,17) = 0;
		WBUFL(buf,21) = 0;
	}

	clif_send(buf,packet_len(WBUFW(buf,0)),dst,SELF);
}

/*==========================================
 * PC�\��
 *------------------------------------------*/
static void clif_getareachar_pc(struct map_session_data* sd,struct map_session_data* dstsd)
{
	int gmlvl;
	struct block_list *d_bl;
	struct guild* g = guild_search(sd->status.guild_id);//(struct guild*) malloc(sizeof(struct guild));
	int i = 0, c = 0;

	if(dstsd->chatID)
	{
		struct chat_data *cd;
		cd=(struct chat_data*)map_id2bl(dstsd->chatID);
		if(cd && cd->usersd[0]==dstsd)
			clif_dispchat(cd,sd->fd);
	}

	if( dstsd->state.vending )
		clif_showvendingboard(&dstsd->bl,dstsd->message,sd->fd);

	if( dstsd->state.buyingstore )
		clif_buyingstore_entry_single(sd, dstsd);

	if( dstsd->status.guild_id && sd->status.guild_id ) {
		for( i = c = 0; i < MAX_GUILDALLIANCE; i++ ) {
			struct guild_alliance *a=&g->alliance[i];
			if( a->guild_id ) {
				if( a->guild_id == dstsd->status.guild_id) {
					c++;
				}
			}
			i++;
		}
	}

	if(dstsd->spiritball > 0)
		clif_spiritball_single(sd->fd, dstsd);

	if( (sd->state.seeghp && ((dstsd->status.guild_id == sd->status.guild_id) || (c)) && sd->status.guild_id) || //guildmaster with @seeghp
		(sd->status.party_id && dstsd->status.party_id == sd->status.party_id) || //Party-mate, or hpdisp setting.
		(sd->bg_id && sd->bg_id == dstsd->bg_id) || //BattleGround
		(battle_config.disp_hpmeter && (gmlvl = pc_isGM(sd)) >= battle_config.disp_hpmeter && gmlvl >= pc_isGM(dstsd)) )
		clif_hpmeter_single(sd->fd, dstsd->bl.id, dstsd->battle_status.hp, dstsd->battle_status.max_hp);

	// display link (sd - dstsd) to sd
	ARR_FIND( 0, 5, i, sd->devotion[i] == dstsd->bl.id );
	if( i < 5 ) clif_devotion(&sd->bl, sd);
	// display links (dstsd - devotees) to sd
	ARR_FIND( 0, 5, i, dstsd->devotion[i] > 0 );
	if( i < 5 ) clif_devotion(&dstsd->bl, sd);
	// display link (dstsd - crusader) to sd
	if( dstsd->sc.data[SC_DEVOTION] && (d_bl = map_id2bl(dstsd->sc.data[SC_DEVOTION]->val1)) != NULL )
		clif_devotion(d_bl, sd);
}

void clif_getareachar_unit(struct map_session_data* sd,struct block_list *bl)
{
	uint8 buf[128];
	struct unit_data *ud;
	struct view_data *vd;
	struct map_session_data *tsd;
	int len;
	
	vd = status_get_viewdata(bl);
	if (!vd || vd->class_ == INVISIBLE_CLASS)
		return;

	tsd = BL_CAST(BL_PC,bl);
	if( battle_config.anti_mayapurple_hack && tsd && tsd->sc.option&(OPTION_HIDE|OPTION_CLOAK) && !tsd->state.evade_antiwpefilter && !sd->special_state.intravision && !sd->sc.data[SC_INTRAVISION] )
		return;

	ud = unit_bl2ud(bl);
	len = ( ud && ud->walktimer != INVALID_TIMER ) ? clif_set_unit_walking(bl,ud,buf) : clif_set_unit_idle(bl,buf,false);
	clif_send(buf,len,&sd->bl,SELF);

	if (vd->cloth_color)
		clif_refreshlook(&sd->bl,bl->id,LOOK_CLOTHES_COLOR,vd->cloth_color,SELF);

	switch (bl->type)
	{
	case BL_PC:
		{
			tsd = (TBL_PC*)bl;
			clif_getareachar_pc(sd, tsd);
			if(tsd->state.size==2) // tiny/big players [Valaris]
				clif_specialeffect_single(bl,423,sd->fd);
			else if(tsd->state.size==1)
				clif_specialeffect_single(bl,421,sd->fd);
			if( tsd->sc.count )
			{
				if( tsd->sc.data[SC_BANDING] )
					clif_display_banding(&sd->bl,&tsd->bl,tsd->sc.data[SC_BANDING]->val1);
				if( tsd->sc.data[SC_ALL_RIDING] )
					clif_display_riding(&sd->bl,&tsd->bl);

			}
			clif_sendaurastoone(tsd, sd);
			if( !battle_config.bg_eAmod_mode && tsd->bg_id && map[tsd->bl.m].flag.battleground )
				clif_sendbgemblem_single(sd->fd,tsd);
		}
		break;
	case BL_MER: // Devotion Effects
		if( ((TBL_MER*)bl)->devotion_flag )
			clif_devotion(bl, sd);
		break;
	case BL_NPC:
		{
			TBL_NPC* nd = (TBL_NPC*)bl;
			if( nd->chat_id )
				clif_dispchat((struct chat_data*)map_id2bl(nd->chat_id),sd->fd);
			if( nd->size == 2 )
				clif_specialeffect_single(bl,423,sd->fd);
			else if( nd->size == 1 )
				clif_specialeffect_single(bl,421,sd->fd);
		}
		break;
	case BL_MOB:
		{
			TBL_MOB* md = (TBL_MOB*)bl;
			if(md->special_state.size==2) // tiny/big mobs [Valaris]
				clif_specialeffect_single(bl,423,sd->fd);
			else if(md->special_state.size==1)
				clif_specialeffect_single(bl,421,sd->fd);
			if (md->option.hp_show == 1)
				clif_hpmeter_single(sd->fd, md->bl.id, md->status.hp, md->status.max_hp);
		}
		break;
	case BL_PET:
		if (vd->head_bottom)
			clif_pet_equip(sd, (TBL_PET*)bl); // needed to display pet equip properly
		break;
	}
}

//Modifies the type of damage according to status changes [Skotlex]
//Aegis data specifies that: 4 endure against single hit sources, 9 against multi-hit.
static inline int clif_calc_delay(int type, int div, int damage, int delay)
{
	return ( delay == 0 && damage > 0 ) ? ( div > 1 ? 9 : 4 ) : type;
}

/*==========================================
 * Estimates walk delay based on the damage criteria. [Skotlex]
 *------------------------------------------*/
static int clif_calc_walkdelay(struct block_list *bl,int delay, int type, int damage, int div_)
{
	if (type == 4 || type == 9 || damage <=0)
		return 0;
	
	if( bl->type == BL_PC )
	{
		if( battle_config.pc_walk_delay_rate != 100 )
			delay = delay * battle_config.pc_walk_delay_rate / 100;
	}
	else if( bl->type == BL_MOB && status_get_mode(bl)&MD_BOSS )
	{
		if( battle_config.walk_delay_rate_boss != 100 )
			delay = delay * battle_config.walk_delay_rate_boss / 100;
	}
	else if( battle_config.walk_delay_rate != 100 )
		delay = delay * battle_config.walk_delay_rate / 100;
	
	if (div_ > 1) //Multi-hit skills mean higher delays.
		delay += battle_config.multihit_delay*(div_-1);

	return delay>0?delay:1; //Return 1 to specify there should be no noticeable delay, but you should stop walking.
}

/*==========================================
 * Sends a 'damage' packet (src performs action on dst)
 * R 008a <src ID>.L <dst ID>.L <server tick>.L <src speed>.L <dst speed>.L <damage>.W <div>.W <type>.B <damage2>.W (ZC_NOTIFY_ACT)
 * R 02e1 <src ID>.L <dst ID>.L <server tick>.L <src speed>.L <dst speed>.L <damage>.L <div>.W <type>.B <damage2>.L (ZC_NOTIFY_ACT2)
 * 
 * type=00 damage [param1: total damage, param2: div, param3: assassin dual-wield damage]
 * type=01 pick up item
 * type=02 sit down
 * type=03 stand up
 * type=04 reflected/absorbed damage?
 * type=08 double attack
 * type=09 don't display flinch animation (endure)
 * type=0a critical hit
 * type=0b lucky dodge
 *------------------------------------------*/
int clif_damage(struct block_list* src, struct block_list* dst, unsigned int tick, int sdelay, int ddelay, int damage, int div, int type, int damage2)
{
	unsigned char buf[33];
	struct status_change *sc;
#if PACKETVER < 20071113
	const int cmd = 0x8a;
#else
	const int cmd = 0x2e1;
#endif

	nullpo_ret(src);
	nullpo_ret(dst);

	pc_record_maxdamage(src, dst, damage + damage2);

	type = clif_calc_delay(type,div,damage+damage2,ddelay);
	sc = status_get_sc(dst);
	if(sc && sc->count) {
		if(sc->data[SC_HALLUCINATION]) {
			if(damage) damage = damage*(sc->data[SC_HALLUCINATION]->val2) + rand()%100;
			if(damage2) damage2 = damage2*(sc->data[SC_HALLUCINATION]->val2) + rand()%100;
		}
	}

	WBUFW(buf,0)=cmd;
	WBUFL(buf,2)=src->id;
	WBUFL(buf,6)=dst->id;
	WBUFL(buf,10)=tick;
	WBUFL(buf,14)=sdelay;
	WBUFL(buf,18)=ddelay;
#if PACKETVER < 20071113
	if (battle_config.hide_woe_damage && map_flag_gvg(src->m)) {
		WBUFW(buf,22)=damage?div:0;
		WBUFW(buf,27)=damage2?div:0;
	} else {
		WBUFW(buf,22)=min(damage, INT16_MAX);
		WBUFW(buf,27)=damage2;
	}
	WBUFW(buf,24)=div;
	WBUFB(buf,26)=type;
#else
	if (battle_config.hide_woe_damage && map_flag_gvg(src->m)) {
		WBUFL(buf,22)=damage?div:0;
		WBUFL(buf,29)=damage2?div:0;
	} else {
		WBUFL(buf,22)=damage;
		WBUFL(buf,29)=damage2;
	}
	WBUFW(buf,26)=div;
	WBUFB(buf,28)=type;
#endif
	if(disguised(dst)) {
		clif_send(buf,packet_len(cmd),dst,AREA_WOS);
		WBUFL(buf,6) = -dst->id;
		clif_send(buf,packet_len(cmd),dst,SELF);
	} else
		clif_send(buf,packet_len(cmd),dst,AREA);

	if(disguised(src)) {
		WBUFL(buf,2) = -src->id;
		if (disguised(dst))
			WBUFL(buf,6) = dst->id;
		if(damage > 0) WBUFW(buf,22) = -1;
#if PACKETVER < 20071113
		if(damage2 > 0) WBUFW(buf,27) = -1;
#else
		if(damage2 > 0) WBUFW(buf,29) = -1;
#endif
		clif_send(buf,packet_len(cmd),src,SELF);
	}
	//Return adjusted can't walk delay for further processing.
	return clif_calc_walkdelay(dst,ddelay,type,damage+damage2,div);
}

/*==========================================
 * src picks up dst
 *------------------------------------------*/
void clif_takeitem(struct block_list* src, struct block_list* dst)
{
	//clif_damage(src,dst,0,0,0,0,0,1,0);
	unsigned char buf[32];

	nullpo_retv(src);
	nullpo_retv(dst);

	WBUFW(buf, 0) = 0x8a;
	WBUFL(buf, 2) = src->id;
	WBUFL(buf, 6) = dst->id;
	WBUFB(buf,26) = 1;
	clif_send(buf, packet_len(0x8a), src, AREA);

}

/*==========================================
 * inform clients in area that `bl` is sitting
 *------------------------------------------*/
void clif_sitting(struct block_list* bl, bool area)
{
	unsigned char buf[32];
	nullpo_retv(bl);

	WBUFW(buf, 0) = 0x8a;
	WBUFL(buf, 2) = bl->id;
	WBUFB(buf,26) = 2;
	if( area )
		clif_send(buf, packet_len(0x8a), bl, AREA);
	else
		clif_send(buf, packet_len(0x8a), bl, SELF);

	if(disguised(bl)) {
		WBUFL(buf, 2) = - bl->id;
		clif_send(buf, packet_len(0x8a), bl, SELF);
	}
}

/*==========================================
 * inform clients in area that `bl` is standing
 *------------------------------------------*/
void clif_standing(struct block_list* bl, bool area)
{
	unsigned char buf[32];
	nullpo_retv(bl);

	WBUFW(buf, 0) = 0x8a;
	WBUFL(buf, 2) = bl->id;
	WBUFB(buf,26) = 3;
	if( area )
		clif_send(buf, packet_len(0x8a), bl, AREA);
	else
		clif_send(buf, packet_len(0x8a), bl, SELF);

	if(disguised(bl)) {
		WBUFL(buf, 2) = - bl->id;
		clif_send(buf, packet_len(0x8a), bl, SELF);
	}
}

/*==========================================
 * Inform client(s) about a map-cell change
 *------------------------------------------*/
void clif_changemapcell(int fd, int m, int x, int y, int type, enum send_target target)
{
	unsigned char buf[32];

	WBUFW(buf,0) = 0x192;
	WBUFW(buf,2) = x;
	WBUFW(buf,4) = y;
	WBUFW(buf,6) = type;
	mapindex_getmapname_ext(map[m].name,(char*)WBUFP(buf,8));

	if( fd )
	{
		WFIFOHEAD(fd,packet_len(0x192));
		memcpy(WFIFOP(fd,0), buf, packet_len(0x192));
		WFIFOSET(fd,packet_len(0x192));
	}
	else
	{
		struct block_list dummy_bl;
		dummy_bl.type = BL_NUL;
		dummy_bl.x = x;
		dummy_bl.y = y;
		dummy_bl.m = m;
		clif_send(buf,packet_len(0x192),&dummy_bl,target);
	}
}

/*==========================================
 *
 *------------------------------------------*/
void clif_getareachar_item(struct map_session_data* sd,struct flooritem_data* fitem)
{
	int view,fd;
	fd=sd->fd;
	//009d <ID>.l <item ID>.w <identify flag>.B <X>.w <Y>.w <amount>.w <subX>.B <subY>.B
	WFIFOHEAD(fd,packet_len(0x9d));

	WFIFOW(fd,0)=0x9d;
	WFIFOL(fd,2)=fitem->bl.id;
	if((view = itemdb_viewid(fitem->item_data.nameid)) > 0)
		WFIFOW(fd,6)=view;
	else
		WFIFOW(fd,6)=fitem->item_data.nameid;
	WFIFOB(fd,8)=fitem->item_data.identify;
	WFIFOW(fd,9)=fitem->bl.x;
	WFIFOW(fd,11)=fitem->bl.y;
	WFIFOW(fd,13)=fitem->item_data.amount;
	WFIFOB(fd,15)=fitem->subx;
	WFIFOB(fd,16)=fitem->suby;
	WFIFOSET(fd,packet_len(0x9d));
}

/*==========================================
 * �ꏊ�X�L���G�t�F�N�g�����E�ɓ���
 *------------------------------------------*/
static void clif_getareachar_skillunit(struct map_session_data *sd, struct skill_unit *unit)
{
	int fd = sd->fd;

#if PACKETVER >= 3
	if(unit->group->unit_id==UNT_GRAFFITI)	{ // Graffiti [Valaris]
		WFIFOHEAD(fd,packet_len(0x1c9));
		WFIFOW(fd, 0)=0x1c9;
		WFIFOL(fd, 2)=unit->bl.id;
		WFIFOL(fd, 6)=unit->group->src_id;
		WFIFOW(fd,10)=unit->bl.x;
		WFIFOW(fd,12)=unit->bl.y;
		WFIFOB(fd,14)=unit->group->unit_id;
		WFIFOB(fd,15)=1;
		WFIFOB(fd,16)=1;
		safestrncpy((char*)WFIFOP(fd,17),unit->group->valstr,MESSAGE_SIZE);
		WFIFOSET(fd,packet_len(0x1c9));
		return;
	}
#endif
	WFIFOHEAD(fd,packet_len(0x11f));
	WFIFOW(fd, 0)=0x11f;
	WFIFOL(fd, 2)=unit->bl.id;
	WFIFOL(fd, 6)=unit->group->src_id;
	WFIFOW(fd,10)=unit->bl.x;
	WFIFOW(fd,12)=unit->bl.y;
	if (battle_config.traps_setting&1 && skill_get_inf2(unit->group->skill_id)&INF2_TRAP)
		WFIFOB(fd,14)=UNT_DUMMYSKILL; //Use invisible unit id for traps.
	else
		WFIFOB(fd,14)=unit->group->unit_id;
	WFIFOB(fd,15)=1; // ignored by client (always gets set to 1)
	WFIFOSET(fd,packet_len(0x11f));

	if(unit->group->skill_id == WZ_ICEWALL)
		clif_changemapcell(fd,unit->bl.m,unit->bl.x,unit->bl.y,5,SELF);
}

/*==========================================
 * �ꏊ�X�L���G�t�F�N�g�����E���������
 *------------------------------------------*/
static void clif_clearchar_skillunit(struct skill_unit *unit, int fd)
{
	nullpo_retv(unit);

	WFIFOHEAD(fd,packet_len(0x120));
	WFIFOW(fd, 0)=0x120;
	WFIFOL(fd, 2)=unit->bl.id;
	WFIFOSET(fd,packet_len(0x120));

	if(unit->group && unit->group->skill_id == WZ_ICEWALL)
		clif_changemapcell(fd,unit->bl.m,unit->bl.x,unit->bl.y,unit->val2,SELF);
}

/*==========================================
 * �ꏊ�X�L���G�t�F�N�g�폜
 *------------------------------------------*/
void clif_skill_delunit(struct skill_unit *unit)
{
	unsigned char buf[16];

	nullpo_retv(unit);

	WBUFW(buf, 0)=0x120;
	WBUFL(buf, 2)=unit->bl.id;
	clif_send(buf,packet_len(0x120),&unit->bl,AREA);
}

/*==========================================
 * Sent when an object gets ankle-snared (ZC_SKILL_UPDATE)
 * Only affects units with class [139,153] client-side
 * R 01ac <object id>.l
 *------------------------------------------*/
void clif_skillunit_update(struct block_list* bl)
{
	unsigned char buf[6];
	nullpo_retv(bl);

	WBUFW(buf,0) = 0x1ac;
	WBUFL(buf,2) = bl->id;

	clif_send(buf,packet_len(0x1ac),bl,AREA);
}

/*==========================================
 *
 *------------------------------------------*/
 int clif_getareachar(struct block_list* bl,va_list ap)
{
	struct map_session_data *sd;

	nullpo_ret(bl);

	sd=va_arg(ap,struct map_session_data*);

	if (sd == NULL || !sd->fd)
		return 0;

	switch(bl->type){
	case BL_ITEM:
		clif_getareachar_item(sd,(struct flooritem_data*) bl);
		break;
	case BL_SKILL:
		clif_getareachar_skillunit(sd,(TBL_SKILL*)bl);
		break;
	default:
		if(&sd->bl == bl)
			break;
		clif_getareachar_unit(sd,bl);
		break;
	}
	return 0;
}

/*==========================================
 * tbl has gone out of view-size of bl
 *------------------------------------------*/
int clif_outsight(struct block_list *bl,va_list ap)
{
	struct block_list *tbl;
	struct view_data *vd;
	TBL_PC *sd, *tsd;
	tbl=va_arg(ap,struct block_list*);
	if(bl == tbl) return 0;
	sd = BL_CAST(BL_PC, bl);
	tsd = BL_CAST(BL_PC, tbl);

	if (tsd && tsd->fd)
	{	//tsd has lost sight of the bl object.
		switch(bl->type){
		case BL_PC:
			if (sd->vd.class_ != INVISIBLE_CLASS)
				clif_clearunit_single(bl->id,CLR_OUTSIGHT,tsd->fd);
			if(sd->chatID){
				struct chat_data *cd;
				cd=(struct chat_data*)map_id2bl(sd->chatID);
				if(cd->usersd[0]==sd)
					clif_dispchat(cd,tsd->fd);
			}
			if( sd->state.vending )
				clif_closevendingboard(bl,tsd->fd);
			if( sd->state.buyingstore )
				clif_buyingstore_disappear_entry_single(tsd, sd);
			break;
		case BL_ITEM:
			clif_clearflooritem((struct flooritem_data*)bl,tsd->fd);
			break;
		case BL_SKILL:
			clif_clearchar_skillunit((struct skill_unit *)bl,tsd->fd);
			break;
		default:
			if ((vd=status_get_viewdata(bl)) && vd->class_ != INVISIBLE_CLASS)
				clif_clearunit_single(bl->id,CLR_OUTSIGHT,tsd->fd);
			break;
		}
	}
	if (sd && sd->fd)
	{	//sd is watching tbl go out of view.
		if ((vd=status_get_viewdata(tbl)) && vd->class_ != INVISIBLE_CLASS)
			clif_clearunit_single(tbl->id,CLR_OUTSIGHT,sd->fd);
	}
	return 0;
}

/*==========================================
 * tbl has come into view of bl
 *------------------------------------------*/
int clif_insight(struct block_list *bl,va_list ap)
{
	struct block_list *tbl;
	TBL_PC *sd, *tsd;
	tbl=va_arg(ap,struct block_list*);

	if (bl == tbl) return 0;
	
	sd = BL_CAST(BL_PC, bl);
	tsd = BL_CAST(BL_PC, tbl);

	if (tsd && tsd->fd)
	{	//Tell tsd that bl entered into his view
		switch(bl->type){
		case BL_ITEM:
			clif_getareachar_item(tsd,(struct flooritem_data*)bl);
			break;
		case BL_SKILL:
			clif_getareachar_skillunit(tsd,(TBL_SKILL*)bl);
			break;
		default:
			clif_getareachar_unit(tsd,bl);
			break;
		}
	}
	if (sd && sd->fd)
	{	//Tell sd that tbl walked into his view
		clif_getareachar_unit(sd,tbl);
	}
	return 0;
}

/*==========================================
 * Refresh on tbl all bl's around (Currently only used for Intravision + Anti MayaPurple Hack
 *------------------------------------------*/
int clif_insight_bl2tbl(struct block_list *bl,va_list ap)
{
	struct block_list *tbl;
	struct map_session_data *sd, *tsd;

	tbl = va_arg(ap,struct block_list*);
	tsd = BL_CAST(BL_PC,tbl);
	sd = BL_CAST(BL_PC,bl);

	if( bl == tbl || !tsd || !tsd->fd || !sd || !pc_ishiding(sd) )
		return 0;

	clif_getareachar_unit(tsd,bl);
	return 0;
}

/*==========================================
 * Refresh tbl on all bl's around
 *------------------------------------------*/
int clif_insight_tbl2bl(struct block_list *bl,va_list ap)
{
	struct block_list *tbl;
	struct map_session_data *sd;

	tbl = va_arg(ap,struct block_list*);
	sd = BL_CAST(BL_PC,bl);

	if( bl == tbl || tbl->type != BL_PC || !sd || !sd->fd )
		return 0;

	clif_getareachar_unit(sd,tbl);
	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_skillupdateinfo(struct map_session_data *sd,int skillid,int type,int range)
{
	int fd, id, cmd, offset = 0;

	nullpo_ret(sd);

#if PACKETVER < 20090715
	cmd = 0x147;
#else
	cmd = 0x7e1;
#endif

	fd = sd->fd;
	if( (id=sd->status.skill[skillid].id) <= 0 )
		return 0;
	WFIFOHEAD(fd,packet_len(cmd));
	WFIFOW(fd,0) = cmd;
	WFIFOW(fd,2) = id;
	if( type )
		WFIFOL(fd,4) = type;
	else
		WFIFOL(fd,4) = skill_get_inf(id);
	WFIFOW(fd,8) = sd->status.skill[skillid].lv;
	WFIFOW(fd,10) = skill_get_sp(id,sd->status.skill[skillid].lv);
	if( range )
		WFIFOW(fd,12) = range;
	else
		WFIFOW(fd,12) = skill_get_range2(&sd->bl, id,sd->status.skill[skillid].lv);

#if PACKETVER < 20090715
	safestrncpy((char*)WFIFOP(fd,14), skill_get_name(id), NAME_LENGTH);
	offset = 24;
#endif
	if(sd->status.skill[skillid].flag ==0)
		WFIFOB(fd,14+offset)= (sd->status.skill[skillid].lv < skill_tree_get_max(id, sd->status.class_))? 1:0;
	else
		WFIFOB(fd,14+offset) = 0;
	WFIFOSET(fd,packet_len(cmd));

	return 0;
}

/*==========================================
 * �X�L�����X�g�𑗐M����
 *------------------------------------------*/
int clif_skillinfoblock(struct map_session_data *sd)
{
	int fd;
	int i,len,id;

	nullpo_ret(sd);

	fd=sd->fd;
	if (!fd) return 0;

	WFIFOHEAD(fd, MAX_SKILL * 37 + 4);
	WFIFOW(fd,0) = 0x10f;
	for ( i = 0, len = 4; i < MAX_SKILL; i++)
	{
		if( (id = sd->status.skill[i].id) != 0 )
		{
			WFIFOW(fd,len)   = id;
			WFIFOW(fd,len+2) = skill_get_inf(id);
			WFIFOW(fd,len+4) = 0;
			WFIFOW(fd,len+6) = sd->status.skill[i].lv;
			WFIFOW(fd,len+8) = skill_get_sp(id,sd->status.skill[i].lv);
			WFIFOW(fd,len+10)= skill_get_range2(&sd->bl, id,sd->status.skill[i].lv);
			safestrncpy((char*)WFIFOP(fd,len+12), skill_get_name(id), NAME_LENGTH);
			if(sd->status.skill[i].flag == SKILL_FLAG_PERMANENT)
				WFIFOB(fd,len+36) = (sd->status.skill[i].lv < skill_tree_get_max(id, sd->status.class_))? 1:0;
			else
				WFIFOB(fd,len+36) = 0;
			len += 37;
		}
	}
	WFIFOW(fd,2)=len;
	WFIFOSET(fd,len);

	return 1;
}

int clif_addskill(struct map_session_data *sd, int id )
{
	int fd;

	nullpo_ret(sd);

	fd = sd->fd;
	if (!fd) return 0;

	if( sd->status.skill[id].id <= 0 )
		return 0;

	WFIFOHEAD(fd, packet_len(0x111));
	WFIFOW(fd,0) = 0x111;
	WFIFOW(fd,2) = id;
	WFIFOW(fd,4) = skill_get_inf(id);
 	WFIFOW(fd,6) = 0;
	WFIFOW(fd,8) = sd->status.skill[id].lv;
	WFIFOW(fd,10) = skill_get_sp(id,sd->status.skill[id].lv);
	WFIFOW(fd,12) = skill_get_range2(&sd->bl, id,sd->status.skill[id].lv);
	safestrncpy((char*)WFIFOP(fd,14), skill_get_name(id), NAME_LENGTH);
	if( sd->status.skill[id].flag == SKILL_FLAG_PERMANENT )
		WFIFOB(fd,38) = (sd->status.skill[id].lv < skill_tree_get_max(id, sd->status.class_))? 1:0;
	else
		WFIFOB(fd,38) = 0;
	WFIFOSET(fd,packet_len(0x111));

	return 1;
}

int clif_deleteskill(struct map_session_data *sd, int id)
{
#if PACKETVER >= 20081217
	int fd;

	nullpo_ret(sd);
	fd = sd->fd;
	if( !fd ) return 0;

	WFIFOHEAD(fd,packet_len(0x441));
	WFIFOW(fd,0) = 0x441;
	WFIFOW(fd,2) = id;
	WFIFOSET(fd,packet_len(0x441));
	return 1;
#else
	return clif_skillinfoblock(sd);
#endif
}

/*==========================================
 * �X�L������U��ʒm
 *------------------------------------------*/
int clif_skillup(struct map_session_data *sd,int skill_num)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x10e));
	WFIFOW(fd,0) = 0x10e;
	WFIFOW(fd,2) = skill_num;
	WFIFOW(fd,4) = sd->status.skill[skill_num].lv;
	WFIFOW(fd,6) = skill_get_sp(skill_num,sd->status.skill[skill_num].lv);
	WFIFOW(fd,8) = skill_get_range2(&sd->bl,skill_num,sd->status.skill[skill_num].lv);
	WFIFOB(fd,10) = (sd->status.skill[skill_num].lv < skill_tree_get_max(sd->status.skill[skill_num].id, sd->status.class_)) ? 1 : 0;
	WFIFOSET(fd,packet_len(0x10e));

	return 0;
}

//PACKET_ZC_SKILLINFO_UPDATE2
//Like packet 0x0x10e, but also contains inf information
void clif_skillinfo(struct map_session_data *sd,int skill, int inf)
{
	const int fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0x7e1));
	WFIFOW(fd,0) = 0x7e1;
	WFIFOW(fd,2) = skill;
	WFIFOL(fd,4) = inf?inf:skill_get_inf(skill);
	WFIFOW(fd,8) = sd->status.skill[skill].lv;
	WFIFOW(fd,10) = skill_get_sp(skill,sd->status.skill[skill].lv);
	WFIFOW(fd,12) = skill_get_range2(&sd->bl,skill,sd->status.skill[skill].lv);
	if( sd->status.skill[skill].flag == SKILL_FLAG_PERMANENT )
		WFIFOB(fd,14) = (sd->status.skill[skill].lv < skill_tree_get_max(skill, sd->status.class_))? 1:0;
	else
		WFIFOB(fd,14) = 0;
	WFIFOSET(fd,packet_len(0x7e1));
}

/// Notifies clients, that an object is about to use a skill (ZC_USESKILL_ACK/ZC_USESKILL_ACK2)
/// 013e <src id>.L <dst id>.L <x pos>.W <y pos>.W <skill id>.W <property>.L <delaytime>.L
/// 07fb <src id>.L <dst id>.L <x pos>.W <y pos>.W <skill id>.W <property>.L <delaytime>.L <is disposable>.B
/// property:
///     0 = Yellow cast aura
///     1 = Water elemental cast aura
///     2 = Earth elemental cast aura
///     3 = Fire elemental cast aura
///     4 = Wind elemental cast aura
///     5 = Poison elemental cast aura
///     6 = Holy elemental cast aura
///     ? = like 0
/// is disposable:
///     0 = yellow chat text "[src name] will use skill [skill name]."
///     1 = no text
void clif_skillcasting(struct block_list* bl, int src_id, int dst_id, int dst_x, int dst_y, int skill_num, int property, int casttime)
{
#if PACKETVER < 20091124
	const int cmd = 0x13e;
#else
	const int cmd = 0x7fb;
#endif
	unsigned char buf[32];

	WBUFW(buf,0) = cmd;
	WBUFL(buf,2) = src_id;
	WBUFL(buf,6) = dst_id;
	WBUFW(buf,10) = dst_x;
	WBUFW(buf,12) = dst_y;
	WBUFW(buf,14) = skill_num;
	WBUFL(buf,16) = property<0?0:property; //Avoid sending negatives as element [Skotlex]
	WBUFL(buf,20) = casttime;
#if PACKETVER >= 20091124
	WBUFB(buf,24) = 1;  // isDisposable
#endif

	if (disguised(bl)) {
		clif_send(buf,packet_len(cmd), bl, AREA_WOS);
		WBUFL(buf,2) = -src_id;
		clif_send(buf,packet_len(cmd), bl, SELF);
	} else
		clif_send(buf,packet_len(cmd), bl, AREA);
}

/*==========================================
 *
 *------------------------------------------*/
int clif_skillcastcancel(struct block_list* bl)
{
	unsigned char buf[16];

	nullpo_ret(bl);

	WBUFW(buf,0) = 0x1b9;
	WBUFL(buf,2) = bl->id;
	clif_send(buf,packet_len(0x1b9), bl, AREA);

	return 0;
}


/// only when type==0:
///  if(skill_id==NV_BASIC)
///   btype==0 "skill failed" MsgStringTable[159]
///   btype==1 "no emotions" MsgStringTable[160]
///   btype==2 "no sit" MsgStringTable[161]
///   btype==3 "no chat" MsgStringTable[162]
///   btype==4 "no party" MsgStringTable[163]
///   btype==5 "no shout" MsgStringTable[164]
///   btype==6 "no PKing" MsgStringTable[165]
///   btype==7 "no alligning" MsgStringTable[383]
///   btype>=8: ignored
///  if(skill_id==AL_WARP) "not enough skill level" MsgStringTable[214]
///  if(skill_id==TF_STEAL) "steal failed" MsgStringTable[205]
///  if(skill_id==TF_POISON) "envenom failed" MsgStringTable[207]
///  otherwise "skill failed" MsgStringTable[204]
/// btype irrelevant
///  type==1 "insufficient SP" MsgStringTable[202]
///  type==2 "insufficient HP" MsgStringTable[203]
///  type==3 "insufficient materials" MsgStringTable[808]
///  type==4 "there is a delay after using a skill" MsgStringTable[219]
///  type==5 "insufficient zeny" MsgStringTable[233]
///  type==6 "wrong weapon" MsgStringTable[239]
///  type==7 "red gemstone needed" MsgStringTable[246]
///  type==8 "blue gemstone needed" MsgStringTable[247]
///  type==9 "overweight" MsgStringTable[580]
///  type==10 "skill failed" MsgStringTable[285]
///  type>=11 ignored
///
/// if(success!=0) doesn't display any of the previous messages
/// Note: when this packet is received an unknown flag is always set to 0,
/// suggesting this is an ACK packet for the UseSkill packets and should be sent on success too [FlavioJS]
int clif_skill_fail(struct map_session_data *sd,int skill_id,int type,int btype)
{
	int fd;

	if (!sd) {	//Since this is the most common nullpo.... 
		ShowDebug("clif_skill_fail: Error, received NULL sd for skill %d\n", skill_id);
		return 0;
	}
	
	fd=sd->fd;
	if (!fd) return 0;

	if(battle_config.display_skill_fail&1)
		return 0; //Disable all skill failed messages

	if(type==0x4 && !sd->state.showdelay)
		return 0; //Disable delay failed messages
	
	if(skill_id == RG_SNATCHER && battle_config.display_skill_fail&4)
		return 0;

	if(skill_id == TF_POISON && battle_config.display_skill_fail&8)
		return 0;

	WFIFOHEAD(fd,packet_len(0x110));
	WFIFOW(fd,0) = 0x110;
	WFIFOW(fd,2) = skill_id;
	WFIFOL(fd,4) = btype;
	WFIFOB(fd,8) = 0;// success
	WFIFOB(fd,9) = type;
	WFIFOSET(fd,packet_len(0x110));

	return 1;
}

int clif_skill_fail2(struct map_session_data *sd,int skill_id,int type,int btype, int val)
{
	int fd;

	if (!sd) {	//Since this is the most common nullpo.... 
		ShowDebug("clif_skill_fail: Error, received NULL sd for skill %d\n", skill_id);
		return 0;
	}
	
	fd=sd->fd;
	if (!fd) return 0;

	if(battle_config.display_skill_fail&1)
		return 0; //Disable all skill failed messages

	if(type==0x4 && !sd->state.showdelay)
		return 0; //Disable delay failed messages
	
	if(skill_id == RG_SNATCHER && battle_config.display_skill_fail&4)
		return 0;

	if(skill_id == TF_POISON && battle_config.display_skill_fail&8)
		return 0;

	WFIFOHEAD(fd,packet_len(0x110));
	WFIFOW(fd,0) = 0x110;
	WFIFOW(fd,2) = skill_id;
	WFIFOW(fd,4) = btype;//btype;
	WFIFOW(fd,6) = val;//val;
	WFIFOB(fd,8) = 0;
	WFIFOB(fd,9) = type;
	WFIFOSET(fd,packet_len(0x110));

	return 1;
}

/*==========================================
 * skill cooldown display icon
 * R 043d <skill ID>.w <tick>.l
 *------------------------------------------*/
int clif_skill_cooldown(struct map_session_data *sd, int skillid, unsigned int tick)
{
#if PACKETVER>=20081112
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x043d));
	WFIFOW(fd,0) = 0x043d;
	WFIFOW(fd,2) = skillid;
	WFIFOL(fd,4) = tick;
	WFIFOSET(fd,packet_len(0x043d));
#endif
	return 0;
}

/*==========================================
 * skill attack effect and damage
 * R 01de <skill ID>.w <src ID>.l <dst ID>.l <tick>.l <src delay>.l <dst delay>.l <damage>.l <skillv>.w <div>.w <type>.B
 *------------------------------------------*/
int clif_skill_damage(struct block_list *src,struct block_list *dst,unsigned int tick,int sdelay,int ddelay,int damage,int div,int skill_id,int skill_lv,int type)
{
	unsigned char buf[64];
	struct status_change *sc;
	struct map_session_data *sd;

	nullpo_ret(src);
	nullpo_ret(dst);

	/* WoE Stats */
	pc_record_maxdamage(src, dst, damage);
	sd = BL_CAST(BL_PC, src);
	if( sd && skill_id == CR_ACIDDEMONSTRATION )
	{
		if( damage > 0 )
		{
			if( sd->status.guild_id && map_allowed_woe(src->m) )
				add2limit(sd->status.wstats.acid_demostration, 1, UINT32_MAX);
			else if( map[src->m].flag.battleground && sd->bg_id )
				add2limit(sd->status.bgstats.acid_demostration, 1, UINT32_MAX);
		}
		else
		{
			if( sd->status.guild_id && map_allowed_woe(src->m) )
				add2limit(sd->status.wstats.acid_demostration_fail, 1, UINT32_MAX);
			else if( map[src->m].flag.battleground && sd->bg_id )
				add2limit(sd->status.bgstats.acid_demostration_fail, 1, UINT32_MAX);
		}
	}

	type = clif_calc_delay(type,div,damage,ddelay);
	sc = status_get_sc(dst);
	if( sc && sc->count && sc->data[SC_HALLUCINATION] && damage )
		damage = damage*(sc->data[SC_HALLUCINATION]->val2) + rand() % 100;

#if PACKETVER < 3
	WBUFW(buf,0)=0x114;
	WBUFW(buf,2)=skill_id;
	WBUFL(buf,4)=src->id;
	WBUFL(buf,8)=dst->id;
	WBUFL(buf,12)=tick;
	WBUFL(buf,16)=sdelay;
	WBUFL(buf,20)=ddelay;
	if (battle_config.hide_woe_damage && map_flag_gvg(src->m)) {
		WBUFW(buf,24)=damage?div:0;
	} else {
		WBUFW(buf,24)=damage;
	}
	WBUFW(buf,26)=skill_lv;
	WBUFW(buf,28)=div;
	WBUFB(buf,30)=type;
	if (disguised(dst)) {
		clif_send(buf,packet_len(0x114),dst,AREA_WOS);
		WBUFL(buf,8)=-dst->id;
		clif_send(buf,packet_len(0x114),dst,SELF);
	} else
		clif_send(buf,packet_len(0x114),dst,AREA);

	if(disguised(src)) {
		WBUFL(buf,4)=-src->id;
		if (disguised(dst)) 
			WBUFL(buf,8)=dst->id;
		if(damage > 0)
			WBUFW(buf,24)=-1;
		clif_send(buf,packet_len(0x114),src,SELF);
	}
#else
	WBUFW(buf,0)=0x1de;
	WBUFW(buf,2)=skill_id;
	WBUFL(buf,4)=src->id;
	WBUFL(buf,8)=dst->id;
	WBUFL(buf,12)=tick;
	WBUFL(buf,16)=sdelay;
	WBUFL(buf,20)=ddelay;
	if (battle_config.hide_woe_damage && map_flag_gvg(src->m)) {
		WBUFL(buf,24)=damage?div:0;
	} else {
		WBUFL(buf,24)=damage;
	}
	WBUFW(buf,28)=skill_lv;
	WBUFW(buf,30)=div;
	WBUFB(buf,32)=type;
	if (disguised(dst)) {
		clif_send(buf,packet_len(0x1de),dst,AREA_WOS);
		WBUFL(buf,8)=-dst->id;
		clif_send(buf,packet_len(0x1de),dst,SELF);
	} else
		clif_send(buf,packet_len(0x1de),dst,AREA);

	if(disguised(src)) {
		WBUFL(buf,4)=-src->id;
		if (disguised(dst))
			WBUFL(buf,8)=dst->id;
		if(damage > 0)
			WBUFL(buf,24)=-1;
		clif_send(buf,packet_len(0x1de),src,SELF);
	}
#endif

	//Because the damage delay must be synced with the client, here is where the can-walk tick must be updated. [Skotlex]
	return clif_calc_walkdelay(dst,ddelay,type,damage,div);
}

/*==========================================
 * ������΂��X�L���U���G�t�F�N�g���_���[�W
 *------------------------------------------*/
/*
int clif_skill_damage2(struct block_list *src,struct block_list *dst,unsigned int tick,int sdelay,int ddelay,int damage,int div,int skill_id,int skill_lv,int type)
{
	unsigned char buf[64];
	struct status_change *sc;

	nullpo_ret(src);
	nullpo_ret(dst);

	type = (type>0)?type:skill_get_hit(skill_id);
	type = clif_calc_delay(type,div,damage,ddelay);
	sc = status_get_sc(dst);

	if(sc && sc->count) {
		if(sc->data[SC_HALLUCINATION] && damage)
			damage = damage*(sc->data[SC_HALLUCINATION]->val2) + rand()%100;
	}

	WBUFW(buf,0)=0x115;
	WBUFW(buf,2)=skill_id;
	WBUFL(buf,4)=src->id;
	WBUFL(buf,8)=dst->id;
	WBUFL(buf,12)=tick;
	WBUFL(buf,16)=sdelay;
	WBUFL(buf,20)=ddelay;
	WBUFW(buf,24)=dst->x;
	WBUFW(buf,26)=dst->y;
	if (battle_config.hide_woe_damage && map_flag_gvg(src->m)) {
		WBUFW(buf,28)=damage?div:0;
	} else {
		WBUFW(buf,28)=damage;
	}
	WBUFW(buf,30)=skill_lv;
	WBUFW(buf,32)=div;
	WBUFB(buf,34)=type;
	clif_send(buf,packet_len(0x115),src,AREA);
	if(disguised(src)) {
		WBUFL(buf,4)=-src->id;
		if(damage > 0)
			WBUFW(buf,28)=-1;
		clif_send(buf,packet_len(0x115),src,SELF);
	}
	if (disguised(dst)) {
		WBUFL(buf,8)=-dst->id;
		if (disguised(src))
			WBUFL(buf,4)=src->id;
		else if(damage > 0)
			WBUFW(buf,28)=-1;
		clif_send(buf,packet_len(0x115),dst,SELF);
	}

	//Because the damage delay must be synced with the client, here is where the can-walk tick must be updated. [Skotlex]
	return clif_calc_walkdelay(dst,ddelay,type,damage,div);
}
*/

/*==========================================
 * �x��/�񕜃X�L���G�t�F�N�g
 *------------------------------------------*/
int clif_skill_nodamage(struct block_list *src,struct block_list *dst,int skill_id,int heal,int fail)
{
	unsigned char buf[32];

	nullpo_ret(dst);

	WBUFW(buf,0)=0x11a;
	WBUFW(buf,2)=skill_id;
	WBUFW(buf,4)=min(heal, INT16_MAX);
	WBUFL(buf,6)=dst->id;
	WBUFL(buf,10)=src?src->id:0;
	WBUFB(buf,14)=fail;

	if (disguised(dst)) {
		clif_send(buf,packet_len(0x11a),dst,AREA_WOS);
		WBUFL(buf,6)=-dst->id;
		clif_send(buf,packet_len(0x11a),dst,SELF);
	} else
		clif_send(buf,packet_len(0x11a),dst,AREA);

	if(src && disguised(src)) {
		WBUFL(buf,10)=-src->id;
		if (disguised(dst))
			WBUFL(buf,6)=dst->id;
		clif_send(buf,packet_len(0x11a),src,SELF);
	}

	return fail;
}

/*==========================================
 * �ꏊ�X�L���G�t�F�N�g
 *------------------------------------------*/
int clif_skill_poseffect(struct block_list *src,int skill_id,int val,int x,int y,int tick)
{
	unsigned char buf[32];

	nullpo_ret(src);

	WBUFW(buf,0)=0x117;
	WBUFW(buf,2)=skill_id;
	WBUFL(buf,4)=src->id;
	WBUFW(buf,8)=val;
	WBUFW(buf,10)=x;
	WBUFW(buf,12)=y;
	WBUFL(buf,14)=tick;
	if(disguised(src)) {
		clif_send(buf,packet_len(0x117),src,AREA_WOS);
		WBUFL(buf,4)=-src->id;
		clif_send(buf,packet_len(0x117),src,SELF);
	} else
		clif_send(buf,packet_len(0x117),src,AREA);

	return 0;
}

/*==========================================
 * �ꏊ�X�L���G�t�F�N�g�\��
 *------------------------------------------*/
//FIXME: this is just an AREA version of clif_getareachar_skillunit()
void clif_skill_setunit(struct skill_unit *unit)
{
	unsigned char buf[128];

	nullpo_retv(unit);

#if PACKETVER >= 3
	if(unit->group->unit_id==UNT_GRAFFITI)	{ // Graffiti [Valaris]
		WBUFW(buf, 0)=0x1c9;
		WBUFL(buf, 2)=unit->bl.id;
		WBUFL(buf, 6)=unit->group->src_id;
		WBUFW(buf,10)=unit->bl.x;
		WBUFW(buf,12)=unit->bl.y;
		WBUFB(buf,14)=unit->group->unit_id;
		WBUFB(buf,15)=1;
		WBUFB(buf,16)=1;
		safestrncpy((char*)WBUFP(buf,17),unit->group->valstr,MESSAGE_SIZE);
		clif_send(buf,packet_len(0x1c9),&unit->bl,AREA);
		return;
	}
#endif
	WBUFW(buf, 0)=0x11f;
	WBUFL(buf, 2)=unit->bl.id;
	WBUFL(buf, 6)=unit->group->src_id;
	WBUFW(buf,10)=unit->bl.x;
	WBUFW(buf,12)=unit->bl.y;
	if (unit->group->state.song_dance&0x1 && unit->val2&UF_ENSEMBLE)
		WBUFB(buf,14)=unit->val2&UF_SONG?UNT_DISSONANCE:UNT_UGLYDANCE;
	else
		WBUFB(buf,14)=unit->group->unit_id;
	WBUFB(buf,15)=1; // ignored by client (always gets set to 1)
	clif_send(buf,packet_len(0x11f),&unit->bl,AREA);
}

/*==========================================
 * ���[�v�ꏊ�I��
 *------------------------------------------*/
void clif_skill_warppoint(struct map_session_data* sd, short skill_num, short skill_lv, unsigned short map1, unsigned short map2, unsigned short map3, unsigned short map4)
{
	int fd;
	nullpo_retv(sd);
	fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x11c));
	WFIFOW(fd,0) = 0x11c;
	WFIFOW(fd,2) = skill_num;
	memset(WFIFOP(fd,4), 0x00, 4*MAP_NAME_LENGTH_EXT);
	if (map1 == (unsigned short)-1) strcpy((char*)WFIFOP(fd,4), "Random");
	else // normal map name
	if (map1 > 0) mapindex_getmapname_ext(mapindex_id2name(map1), (char*)WFIFOP(fd,4));
	if (map2 > 0) mapindex_getmapname_ext(mapindex_id2name(map2), (char*)WFIFOP(fd,20));
	if (map3 > 0) mapindex_getmapname_ext(mapindex_id2name(map3), (char*)WFIFOP(fd,36));
	if (map4 > 0) mapindex_getmapname_ext(mapindex_id2name(map4), (char*)WFIFOP(fd,52));
	WFIFOSET(fd,packet_len(0x11c));

	sd->menuskill_id = skill_num;
	if (skill_num == AL_WARP)
		sd->menuskill_val = (sd->ud.skillx<<16)|sd->ud.skilly; //Store warp position here.
	else
		sd->menuskill_val = skill_lv;
}

/// Memo message.
/// type=0 : "Saved location as a Memo Point for Warp skill." in color 0xFFFF00 (cyan)
/// type=1 : "Skill Level is not high enough." in color 0x0000FF (red)
/// type=2 : "You haven't learned Warp." in color 0x0000FF (red)
///
/// @param sd Who receives the message
/// @param type What message
void clif_skill_memomessage(struct map_session_data* sd, int type)
{
	int fd;

	nullpo_retv(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x11e));
	WFIFOW(fd,0)=0x11e;
	WFIFOB(fd,2)=type;
	WFIFOSET(fd,packet_len(0x11e));
}

/// Teleport message.
/// type=0 : "Unable to Teleport in this area" in color 0xFFFF00 (cyan)
/// type=1 : "Saved point cannot be memorized." in color 0x0000FF (red)
///
/// @param sd Who receives the message
/// @param type What message
void clif_skill_teleportmessage(struct map_session_data *sd, int type)
{
	int fd;

	nullpo_retv(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x189));
	WFIFOW(fd,0)=0x189;
	WFIFOW(fd,2)=type;
	WFIFOSET(fd,packet_len(0x189));
}

/*==========================================
 * �����X�^�[���
 *------------------------------------------*/
int clif_skill_estimation(struct map_session_data *sd,struct block_list *dst)
{
	struct status_data *status;
	unsigned char buf[64];
	int i;//, fix;

	nullpo_ret(sd);
	nullpo_ret(dst);

	if( dst->type != BL_MOB )
		return 0;

	status = status_get_status_data(dst);

	WBUFW(buf, 0)=0x18c;
	WBUFW(buf, 2)=status_get_class(dst);
	WBUFW(buf, 4)=status_get_lv(dst);
	WBUFW(buf, 6)=status->size;
	WBUFL(buf, 8)=status->hp;
	WBUFW(buf,12)= (battle_config.estimation_type&1?status->def:0)
		+(battle_config.estimation_type&2?status->def2:0);
	WBUFW(buf,14)=status->race;
	WBUFW(buf,16)= (battle_config.estimation_type&1?status->mdef:0)
  		+(battle_config.estimation_type&2?status->mdef2:0);
	WBUFW(buf,18)= status->def_ele;
	for(i=0;i<9;i++)
		WBUFB(buf,20+i)= (unsigned char)battle_attr_ratio(i+1,status->def_ele, status->ele_lv);
//		The following caps negative attributes to 0 since the client displays them as 255-fix. [Skotlex]
//		WBUFB(buf,20+i)= (unsigned char)((fix=battle_attr_ratio(i+1,status->def_ele, status->ele_lv))<0?0:fix);

	clif_send(buf,packet_len(0x18c),&sd->bl,sd->status.party_id>0?PARTY_SAMEMAP:SELF);
	return 0;
}
/*==========================================
 * �A�C�e�������\���X�g
 *------------------------------------------*/
int clif_skill_produce_mix_list(struct map_session_data *sd, int skill_num, int trigger)
{
	int i,c,view,fd;
	nullpo_ret(sd);

	if(sd->menuskill_id == skill_num)
		return 0; //Avoid resending the menu twice or more times...
	fd=sd->fd;
	WFIFOHEAD(fd, MAX_SKILL_PRODUCE_DB * 8 + 8);
	WFIFOW(fd, 0)=0x18d;

	for(i=0,c=0;i<MAX_SKILL_PRODUCE_DB;i++){
		if( skill_can_produce_mix(sd,skill_produce_db[i].nameid,trigger, 1) ){
			if((view = itemdb_viewid(skill_produce_db[i].nameid)) > 0)
				WFIFOW(fd,c*8+ 4)= view;
			else
				WFIFOW(fd,c*8+ 4)= skill_produce_db[i].nameid;
			WFIFOW(fd,c*8+ 6)= 0x0012;
			WFIFOL(fd,c*8+ 8)= sd->status.char_id;
			c++;
		}
	}
	WFIFOW(fd, 2)=c*8+8;
	WFIFOSET(fd,WFIFOW(fd,2));
	if(c > 0) {
		sd->menuskill_id = skill_num;
		sd->menuskill_val = trigger;
		return 1;
	}
	return 0;
}

void clif_skill_msg(struct map_session_data *sd, int skill_id, int msg)
{
#if PACKETVER >= 20090922
	int fd;

	nullpo_retv(sd);

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0x7e6));
	WFIFOW(fd,0) = 0x7e6;
	WFIFOW(fd,2) = skill_id;
	WFIFOL(fd,4) = msg;
	WFIFOSET(fd, packet_len(0x7e6));
#endif
}

/*==========================================
 *
 *------------------------------------------*/
void clif_cooking_list(struct map_session_data *sd, int trigger, int skill_id, int qty, int list_type)
{
	int fd;
	int i, c;
	int view;

	nullpo_retv(sd);
	fd = sd->fd;

	WFIFOHEAD(fd, 6 + 2*MAX_SKILL_PRODUCE_DB);
	WFIFOW(fd,0) = 0x25a;
	WFIFOW(fd,4) = list_type; // list type

	c = 0;
	for( i = 0; i < MAX_SKILL_PRODUCE_DB; i++ )
	{
		if( !skill_can_produce_mix(sd,skill_produce_db[i].nameid,trigger, qty) )
			continue;

		if( (view = itemdb_viewid(skill_produce_db[i].nameid)) > 0 )
			WFIFOW(fd, 6+2*c) = view;
		else
			WFIFOW(fd, 6+2*c) = skill_produce_db[i].nameid;

		c++;
	}

	if( skill_id == AM_PHARMACY )
	{	// Only send it while Cooking else check for c.
		WFIFOW(fd,2) = 6 + 2*c;
		WFIFOSET(fd,WFIFOW(fd,2));
	}

	if( c > 0 )
	{
		sd->menuskill_id = skill_id;
		sd->menuskill_val = trigger;
		if( skill_id != AM_PHARMACY )
		{
			sd->menuskill_itemused = qty; // amount.
			WFIFOW(fd,2) = 6 + 2*c;
			WFIFOSET(fd,WFIFOW(fd,2));
		}
	}
	else
	{
		sd->menuskill_id = sd->menuskill_val = sd->menuskill_itemused = 0;
		if( skill_id != AM_PHARMACY ) // AM_PHARMACY is used to Cooking.
		{	// It fails.
#if PACKETVER >= 20090922
			clif_skill_msg(sd,skill_id,1573);
#else
			WFIFOW(fd,2) = 6 + 2*c;
			WFIFOSET(fd,WFIFOW(fd,2));
#endif
		}
	}
}

/*==========================================
 * Sends a status change packet to the object only, used for loading status changes. [Skotlex]
 *------------------------------------------*/
int clif_status_load(struct block_list *bl,int type, int flag)
{
	int fd;
	if (type == SI_BLANK)  //It shows nothing on the client...
		return 0;
	
	if (bl->type != BL_PC)
		return 0;

	fd = ((struct map_session_data*)bl)->fd;
	
	WFIFOHEAD(fd,packet_len(0x196));
	WFIFOW(fd,0)=0x0196;
	WFIFOW(fd,2)=type;
	WFIFOL(fd,4)=bl->id;
	WFIFOB(fd,8)=flag; //Status start
	WFIFOSET(fd, packet_len(0x196));
	return 0;
}
/*==========================================
 * ��Ԉُ�A�C�R��/���b�Z�[�W�\��
 *------------------------------------------*/
int clif_status_change(struct block_list *bl, int type, int flag, unsigned int tick, int val1, int val2, int val3)
{
	unsigned char buf[32];

	if (type == SI_BLANK)  //It shows nothing on the client...
		return 0;

	nullpo_ret(bl);
	switch( type )
	{
	case SI_BLANK:
	case SI_MAXIMIZEPOWER:
	case SI_RIDING:
	case SI_FALCON:
	case SI_TRICKDEAD:
	case SI_BROKENARMOR:
	case SI_BROKENWEAPON:
	case SI_WEIGHT50:
	case SI_WEIGHT90:
	case SI_TENSIONRELAX:
	case SI_LANDENDOW:
	case SI_AUTOBERSERK:
	case SI_BUMP:
	case SI_READYSTORM:
	case SI_READYDOWN:
	case SI_READYTURN:
	case SI_READYCOUNTER:
	case SI_DODGE:
	case SI_DEVIL:
	case SI_NIGHT:
	case SI_INTRAVISION:
	case SI_REPRODUCE:
	case SI_BLOODYLUST:
	case SI_FORCEOFVANGUARD:
	case SI_NEUTRALBARRIER:
	case SI_STEALTHFIELD:
	case SI_BANDING:
	case SI_OVERHEAT:
		tick = 0;
		break;
	}

	// if flag = 0 this should send 0x196.
	if( flag && battle_config.display_status_timers && tick > 0 )
		WBUFW(buf,0)=0x043f;
	else
		WBUFW(buf,0)=0x0196;
	WBUFW(buf,2)=type;
	WBUFL(buf,4)=bl->id;
	WBUFB(buf,8)=flag;
	if( flag && battle_config.display_status_timers && tick>0 )
	{
		WBUFL(buf,9)=tick;
		WBUFL(buf,13)=val1;
		WBUFL(buf,17)=val2;
		WBUFL(buf,21)=val3;
	}
	clif_send(buf,packet_len(WBUFW(buf,0)),bl,AREA);
	return 0;
}


/*==========================================
 * Display Banding when someone under this
 * status change walk into your view range.
 *------------------------------------------*/
void clif_display_banding(struct block_list *dst, struct block_list *bl, int val1)
{
	unsigned char buf[32];

	nullpo_retv(bl);
	nullpo_retv(dst);

	if( battle_config.display_status_timers )
		WBUFW(buf,0)=0x043f;
	else
		WBUFW(buf,0)=0x0196;
	WBUFW(buf,2)=SI_BANDING;
	WBUFL(buf,4)=bl->id;
	WBUFB(buf,8)=1;
	if( battle_config.display_status_timers )
	{
		WBUFL(buf,9)=0;
		WBUFL(buf,13)=val1;
		WBUFL(buf,17)=0;
		WBUFL(buf,21)=0;
	}
	clif_send(buf,packet_len(WBUFW(buf,0)),dst,SELF);
}

/*==========================================
 * Send message (modified by [Yor])
 *------------------------------------------*/
int clif_displaymessage(const int fd, const char* mes)
{
	// invalid pointer?
	nullpo_retr(-1, mes);
	
	//Scrapped, as these are shared by disconnected players =X [Skotlex]
	if (fd == 0)
		return 0;
	else {
		int len_mes = strlen(mes);

		if (len_mes > 0) { // don't send a void message (it's not displaying on the client chat). @help can send void line.
			WFIFOHEAD(fd, 5 + len_mes);
			WFIFOW(fd,0) = 0x8e;
			WFIFOW(fd,2) = 5 + len_mes; // 4 + len + NULL teminate
			memcpy(WFIFOP(fd,4), mes, len_mes + 1);
			WFIFOSET(fd, 5 + len_mes);
		}
	}

	return 0;
}

/*==========================================
 * �V�̐��𑗐M����
 * Send broadcast message in yellow or blue (without font formatting).
 * S 009A <len>.W <message>.?B
 *------------------------------------------*/
int clif_broadcast(struct block_list* bl, const char* mes, int len, int type, enum send_target target)
{
	int            lp  = type ? 4 : 0;
	unsigned char *buf = (unsigned char*)aMallocA((4 + lp + len)*sizeof(unsigned char));

	WBUFW(buf,0) = 0x9a;
	WBUFW(buf,2) = 4 + lp + len;
	if (type == 0x10) // bc_blue
		WBUFL(buf,4) = 0x65756c62; //If there's "blue" at the beginning of the message, game client will display it in blue instead of yellow.
	else if (type == 0x20) // bc_woe
		WBUFL(buf,4) = 0x73737373; //If there's "ssss", game client will recognize message as 'WoE broadcast'.
	memcpy(WBUFP(buf, 4 + lp), mes, len);
	clif_send(buf, WBUFW(buf,2), bl, target);
	
	if (buf)
		aFree(buf);
	return 0;
}

/*==========================================
 * �O���[�o�����b�Z�[�W
 *------------------------------------------*/
void clif_GlobalMessage(struct block_list* bl, const char* message)
{
	char buf[100];
	int len;

	nullpo_retv(bl);

	if(!message)
		return;

	len = strlen(message)+1;

	if( len > sizeof(buf)-8 )
	{
		ShowWarning("clif_GlobalMessage: Truncating too long message '%s' (len=%d).\n", message, len);
		len = sizeof(buf)-8;
	}

	WBUFW(buf,0)=0x8d;
	WBUFW(buf,2)=len+8;
	WBUFL(buf,4)=bl->id;
	safestrncpy((char *) WBUFP(buf,8),message,len);
	clif_send((unsigned char *) buf,WBUFW(buf,2),bl,ALL_CLIENT);
}

/*==========================================
 * Chat Room System [Zephyrus]
 *------------------------------------------*/
void clif_channel_message(struct channel_data *cd, const char *message, int color)
{
	struct map_session_data *pl_sd;
	char mes[CHAT_SIZE_MAX], buf[1024];
	int len, fd;
	unsigned long mcolor; // Message Color
	DBIterator* iter;

	if( !message || !*message || cd == NULL )
		return;
	
	strncpy(mes, message, CHAT_SIZE_MAX);
	len = (int)strlen(message) + 1;
	mes_len_check(mes, len, CHAT_SIZE_MAX);
	mcolor = (color < 0) ? cd->color : channel_color[cap_value(color,0,38)];

	// Convert Color to BGR
	mcolor = (mcolor & 0x0000FF) << 16 | (mcolor & 0x00FF00) | (mcolor & 0xFF0000) >> 16;
	WBUFW(buf,0) = 0x2C1;
	WBUFW(buf,2) = len + 12;
	WBUFL(buf,4) = 0;
	WBUFL(buf,8) = mcolor;
	memcpy(WBUFP(buf,12),mes,len);

	iter = db_iterator(cd->users_db);
	for( pl_sd = (struct map_session_data *)dbi_first(iter); dbi_exists(iter); pl_sd = (struct map_session_data *)dbi_next(iter) )
	{
		if( pl_sd->chatID )
			continue;

		fd = pl_sd->fd;
		WFIFOHEAD(fd, WBUFW(buf,2));
		memcpy(WFIFOP(fd,0), (unsigned char *)buf, WBUFW(buf,2));
		WFIFOSET(fd, WBUFW(buf,2));
	}
	dbi_destroy(iter);

	return;
}

/*==========================================
 * Send broadcast message with font formatting.
 * S 01C3 <len>.W <fontColor>.L <fontType>.W <fontSize>.W <fontAlign>.W <fontY>.W <message>.?B
 *------------------------------------------*/
int clif_broadcast2(struct block_list* bl, const char* mes, int len, unsigned long fontColor, short fontType, short fontSize, short fontAlign, short fontY, enum send_target target)
{
	unsigned char *buf = (unsigned char*)aMallocA((16 + len)*sizeof(unsigned char));

	WBUFW(buf,0)  = 0x1c3;
	WBUFW(buf,2)  = len + 16;
	WBUFL(buf,4)  = fontColor;
	WBUFW(buf,8)  = fontType;
	WBUFW(buf,10) = fontSize;
	WBUFW(buf,12) = fontAlign;
	WBUFW(buf,14) = fontY;
	memcpy(WBUFP(buf,16), mes, len);
	clif_send(buf, WBUFW(buf,2), bl, target);

	if (buf)
		aFree(buf);
	return 0;
}
/*==========================================
 * HPSP�񕜃G�t�F�N�g�𑗐M����
 *------------------------------------------*/
int clif_heal(int fd,int type,int val)
{
	WFIFOHEAD(fd,packet_len(0x13d));
	WFIFOW(fd,0)=0x13d;
	WFIFOW(fd,2)=type;
	WFIFOW(fd,4)=cap_value(val,0,INT16_MAX);
	WFIFOSET(fd,packet_len(0x13d));

	return 0;
}

/*==========================================
 * ��������
 *------------------------------------------*/
int clif_resurrection(struct block_list *bl,int type)
{
	unsigned char buf[16];

	nullpo_ret(bl);

	WBUFW(buf,0)=0x148;
	WBUFL(buf,2)=bl->id;
	WBUFW(buf,6)=type;

	clif_send(buf,packet_len(0x148),bl,type==1 ? AREA : AREA_WOS);
	if (disguised(bl))
		clif_spawn(bl);

	return 0;
}

/// Sets the map property (ZC_NOTIFY_MAPPROPERTY).
void clif_map_property(struct map_session_data* sd, enum map_property property)
{
	int fd;

	nullpo_retv(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x199));
	WFIFOW(fd,0)=0x199;
	WFIFOW(fd,2)=property;
	WFIFOSET(fd,packet_len(0x199));
}

/// Set the map type (ZC_NOTIFY_MAPPROPERTY2)
void clif_map_type(struct map_session_data* sd, enum map_type type)
{
	int fd;

	nullpo_retv(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x1D6));
	WFIFOW(fd,0)=0x1D6;
	WFIFOW(fd,2)=type;
	WFIFOSET(fd,packet_len(0x1D6));
}

/*==========================================
 * PVP�����H(��)
 *------------------------------------------*/
int clif_pvpset(struct map_session_data *sd,int pvprank,int pvpnum,int type)
{
	if(type == 2) {
		int fd = sd->fd;
		WFIFOHEAD(fd,packet_len(0x19a));
		WFIFOW(fd,0) = 0x19a;
		WFIFOL(fd,2) = sd->bl.id;
		WFIFOL(fd,6) = pvprank;
		WFIFOL(fd,10) = pvpnum;
		WFIFOSET(fd,packet_len(0x19a));
	} else {
		unsigned char buf[32];
		WBUFW(buf,0) = 0x19a;
		WBUFL(buf,2) = sd->bl.id;
		if( !battle_config.anti_mayapurple_hack && sd->sc.option&(OPTION_HIDE|OPTION_CLOAK) )
			WBUFL(buf,6) = UINT32_MAX; //On client displays as --
		else
			WBUFL(buf,6) = pvprank;
		WBUFL(buf,10) = pvpnum;
		if( sd->sc.option&OPTION_INVISIBLE || sd->disguise || (battle_config.anti_mayapurple_hack && sd->sc.option&(OPTION_HIDE|OPTION_CLOAK)) )
			clif_send(buf,packet_len(0x19a),&sd->bl,SELF); //Causes crashes when a 'mob' with pvp info dies.
		else if( !type )
			clif_send(buf,packet_len(0x19a),&sd->bl,AREA);
		else
			clif_send(buf,packet_len(0x19a),&sd->bl,ALL_SAMEMAP);
	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
void clif_map_property_mapall(int map, enum map_property property)
{
	struct block_list bl;
	unsigned char buf[16];

	bl.id = 0;
	bl.type = BL_NUL;
	bl.m = map;
	WBUFW(buf,0)=0x199;
	WBUFW(buf,2)=property;
	clif_send(buf,packet_len(0x199),&bl,ALL_SAMEMAP);
}

/*==========================================
 * ���B�G�t�F�N�g�𑗐M����
 *------------------------------------------*/
void clif_refine(int fd, int fail, int index, int val)
{
	WFIFOHEAD(fd,packet_len(0x188));
	WFIFOW(fd,0)=0x188;
	WFIFOW(fd,2)=fail;
	WFIFOW(fd,4)=index+2;
	WFIFOW(fd,6)=val;
	WFIFOSET(fd,packet_len(0x188));
}

/// result=0: "weapon upgrated: %s" MsgStringTable[911] in rgb(0,255,255)
/// result=1: "weapon upgrated: %s" MsgStringTable[912] in rgb(0,205,205)
/// result=2: "cannot upgrade %s until you level up the upgrade weapon skill" MsgStringTable[913] in rgb(255,200,200)
/// result=3: "you lack the item %s to upgrade the weapon" MsgStringTable[914] in rgb(255,200,200)
void clif_upgrademessage(int fd, int result, int item_id)
{
	WFIFOHEAD(fd,packet_len(0x223));
	WFIFOW(fd,0)=0x223;
	WFIFOL(fd,2)=result;
	WFIFOW(fd,6)=item_id;
	WFIFOSET(fd,packet_len(0x223));
}

/*==========================================
 * Wisp/page is transmitted to the destination player
 * R 0097 <len>.w <nick>.24B <message>.?B
 * R 0097 <len>.w <nick>.24B <???>.L <message>.?B
 *------------------------------------------*/
int clif_wis_message(int fd, const char* nick, const char* mes, int mes_len)
{
#if PACKETVER < 20091104
	WFIFOHEAD(fd, mes_len + NAME_LENGTH + 4);
	WFIFOW(fd,0) = 0x97;
	WFIFOW(fd,2) = mes_len + NAME_LENGTH + 4;
	safestrncpy((char*)WFIFOP(fd,4), nick, NAME_LENGTH);
	safestrncpy((char*)WFIFOP(fd,28), mes, mes_len);
	WFIFOSET(fd,WFIFOW(fd,2));
#else
	WFIFOHEAD(fd, mes_len + NAME_LENGTH + 8);
	WFIFOW(fd,0) = 0x97;
	WFIFOW(fd,2) = mes_len + NAME_LENGTH + 8;
	safestrncpy((char*)WFIFOP(fd,4), nick, NAME_LENGTH);
	WFIFOL(fd,28) = 0; // isAdmin; if nonzero, also displays text above char
	safestrncpy((char*)WFIFOP(fd,32), mes, mes_len);
	WFIFOSET(fd,WFIFOW(fd,2));
#endif
	return 0;
}

/*==========================================
 * Inform the player about the result of his whisper action
 * R 0098 <type>.B
 * 0: success to send wisper
 * 1: target character is not loged in
 * 2: ignored by target
 * 3: everyone ignored by target
 *------------------------------------------*/
int clif_wis_end(int fd, int flag)
{
	WFIFOHEAD(fd,packet_len(0x98));
	WFIFOW(fd,0) = 0x98;
	WFIFOW(fd,2) = flag;
	WFIFOSET(fd,packet_len(0x98));
	return 0;
}

/*==========================================
 * �L����ID���O�������ʂ𑗐M����
 *------------------------------------------*/
int clif_solved_charname(int fd, int charid, const char* name)
{
	WFIFOHEAD(fd,packet_len(0x194));
	WFIFOW(fd,0)=0x194;
	WFIFOL(fd,2)=charid;
	safestrncpy((char*)WFIFOP(fd,6), name, NAME_LENGTH);
	WFIFOSET(fd,packet_len(0x194));
	return 0;
}

/*==========================================
 * �J�[�h�̑}���\���X�g��Ԃ�
 *------------------------------------------*/
int clif_use_card(struct map_session_data *sd,int idx)
{
	int i,c,ep;
	int fd=sd->fd;

	nullpo_ret(sd);
	if (idx < 0 || idx >= MAX_INVENTORY) //Crash-fix from bad packets.
		return 0;

	if (!sd->inventory_data[idx] || sd->inventory_data[idx]->type != IT_CARD)
		return 0; //Avoid parsing invalid item indexes (no card/no item)
			
	ep=sd->inventory_data[idx]->equip;
	WFIFOHEAD(fd,MAX_INVENTORY * 2 + 4);
	WFIFOW(fd,0)=0x017b;

	for(i=c=0;i<MAX_INVENTORY;i++){
		int j;

		if(sd->inventory_data[i] == NULL)
			continue;
		if(sd->inventory_data[i]->type!=IT_WEAPON && sd->inventory_data[i]->type!=IT_ARMOR)
			continue;
		if(itemdb_isspecial(sd->status.inventory[i].card[0])) //Can't slot it
			continue;

		if(sd->status.inventory[i].identify==0 )	//Not identified
			continue;

		if((sd->inventory_data[i]->equip&ep)==0)	//Not equippable on this part.
			continue;

		if(sd->inventory_data[i]->type==IT_WEAPON && ep==EQP_SHIELD) //Shield card won't go on left weapon.
			continue;
	
		if( itemdb_isenchant(sd->inventory_data[idx]->nameid) )
		{
			if( sd->inventory_data[i]->slot > 3 )
				continue; // More than 3 slots
			switch( sd->inventory_data[i]->nameid )
			{ // Non Enchantable Stuff
			case 2357:
				continue;
			}
		}
		else
		{
			ARR_FIND( 0, sd->inventory_data[i]->slot, j, sd->status.inventory[i].card[j] == 0 );
			if( j == sd->inventory_data[i]->slot )	// No room
				continue;
		}

		WFIFOW(fd,4+c*2)=i+2;
		c++;
	}
	WFIFOW(fd,2)=4+c*2;
	WFIFOSET(fd,WFIFOW(fd,2));

	return 0;
}
/*==========================================
 * �J�[�h�̑}���I��
 *------------------------------------------*/
int clif_insert_card(struct map_session_data *sd,int idx_equip,int idx_card,int flag)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x17d));
	WFIFOW(fd,0)=0x17d;
	WFIFOW(fd,2)=idx_equip+2;
	WFIFOW(fd,4)=idx_card+2;
	WFIFOB(fd,6)=flag;
	WFIFOSET(fd,packet_len(0x17d));
	return 0;
}

/*==========================================
 * �Ӓ�\�A�C�e�����X�g���M
 *------------------------------------------*/
int clif_item_identify_list(struct map_session_data *sd)
{
	int i,c;
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;

	WFIFOHEAD(fd,MAX_INVENTORY * 2 + 4);
	WFIFOW(fd,0)=0x177;
	for(i=c=0;i<MAX_INVENTORY;i++){
		if(sd->status.inventory[i].nameid > 0 && !sd->status.inventory[i].identify){
			WFIFOW(fd,c*2+4)=i+2;
			c++;
		}
	}
	if(c > 0) {
		WFIFOW(fd,2)=c*2+4;
		WFIFOSET(fd,WFIFOW(fd,2));
		sd->menuskill_id = MC_IDENTIFY;
		sd->menuskill_val = c;
	}
	return 0;
}

/*==========================================
 * �Ӓ茋��
 *------------------------------------------*/
int clif_item_identified(struct map_session_data *sd,int idx,int flag)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x179));
	WFIFOW(fd, 0)=0x179;
	WFIFOW(fd, 2)=idx+2;
	WFIFOB(fd, 4)=flag;
	WFIFOSET(fd,packet_len(0x179));
	return 0;
}

/*==========================================
 * �C���\�A�C�e�����X�g���M
 *------------------------------------------*/
int clif_item_repair_list(struct map_session_data *sd,struct map_session_data *dstsd)
{
	int i,c;
	int fd;
	int nameid;

	nullpo_ret(sd);
	nullpo_ret(dstsd);

	fd=sd->fd;

	WFIFOHEAD(fd, MAX_INVENTORY * 13 + 4);
	WFIFOW(fd,0)=0x1fc;
	for(i=c=0;i<MAX_INVENTORY;i++){
		if((nameid=dstsd->status.inventory[i].nameid) > 0 && dstsd->status.inventory[i].attribute!=0){// && skill_can_repair(sd,nameid)){
			WFIFOW(fd,c*13+4) = i;
			WFIFOW(fd,c*13+6) = nameid;
			WFIFOL(fd,c*13+8) = sd->status.char_id;
			WFIFOL(fd,c*13+12)= dstsd->status.char_id;
			WFIFOB(fd,c*13+16)= c;
			c++;
		}
	}
	if(c > 0) {
		WFIFOW(fd,2)=c*13+4;
		WFIFOSET(fd,WFIFOW(fd,2));
		sd->menuskill_id = BS_REPAIRWEAPON;
		sd->menuskill_val = dstsd->bl.id;
	}else
		clif_skill_fail(sd,sd->ud.skillid,0,0);

	return 0;
}
int clif_item_repaireffect(struct map_session_data *sd,int nameid,int flag)
{
	int view,fd;

	nullpo_ret(sd);
	fd=sd->fd;

	WFIFOHEAD(fd,packet_len(0x1fe));
	WFIFOW(fd, 0)=0x1fe;
	if((view = itemdb_viewid(nameid)) > 0)
		WFIFOW(fd, 2)=view;
	else
		WFIFOW(fd, 2)=nameid;
	WFIFOB(fd, 4)=flag;
	WFIFOSET(fd,packet_len(0x1fe));

	return 0;
}

/*==========================================
 * Weapon Refining - Taken from jAthena
 *------------------------------------------*/
int clif_item_refine_list(struct map_session_data *sd)
{
	int i,c;
	int fd;
	int skilllv;
	int wlv;
	int refine_item[5];

	nullpo_ret(sd);

	skilllv = pc_checkskill(sd,WS_WEAPONREFINE);

	fd=sd->fd;

	refine_item[0] = -1;
	refine_item[1] = pc_search_inventory(sd,1010);
	refine_item[2] = pc_search_inventory(sd,1011);
	refine_item[3] = refine_item[4] = pc_search_inventory(sd,984);

	WFIFOHEAD(fd, MAX_INVENTORY * 13 + 4);
	WFIFOW(fd,0)=0x221;
	for(i=c=0;i<MAX_INVENTORY;i++){
		if(sd->status.inventory[i].nameid > 0 && sd->status.inventory[i].refine < skilllv &&
			sd->status.inventory[i].identify && (wlv=itemdb_wlv(sd->status.inventory[i].nameid)) >=1 &&
			refine_item[wlv]!=-1 && !(sd->status.inventory[i].equip&0x0022)){
			WFIFOW(fd,c*13+ 4)=i+2;
			WFIFOW(fd,c*13+ 6)=sd->status.inventory[i].nameid;
			WFIFOW(fd,c*13+ 8)=0; //TODO: Wonder what are these for? Perhaps ID of weapon's crafter if any?
			WFIFOW(fd,c*13+10)=0;
			WFIFOB(fd,c*13+12)=c;
			c++;
		}
	}
	WFIFOW(fd,2)=c*13+4;
	WFIFOSET(fd,WFIFOW(fd,2));
	if (c > 0) {
		sd->menuskill_id = WS_WEAPONREFINE;
		sd->menuskill_val = skilllv;
	}
	return 0;
}

/*==========================================
 * �A�C�e���ɂ��ꎞ�I�ȃX�L������
 *------------------------------------------*/
int clif_item_skill(struct map_session_data *sd,int skillid,int skilllv)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x147));
	WFIFOW(fd, 0)=0x147;
	WFIFOW(fd, 2)=skillid;
	WFIFOW(fd, 4)=skill_get_inf(skillid);
	WFIFOW(fd, 6)=0;
	WFIFOW(fd, 8)=skilllv;
	WFIFOW(fd,10)=skill_get_sp(skillid,skilllv);
	WFIFOW(fd,12)=skill_get_range2(&sd->bl, skillid,skilllv);
	safestrncpy((char*)WFIFOP(fd,14),skill_get_name(skillid),NAME_LENGTH);
	WFIFOB(fd,38)=0;
	WFIFOSET(fd,packet_len(0x147));
	return 0;
}

/*==========================================
 * �J�[�g�ɃA�C�e���ǉ�
 *------------------------------------------*/
int clif_cart_additem(struct map_session_data *sd,int n,int amount,int fail)
{
	int view,fd;
	unsigned char *buf;

	nullpo_ret(sd);

	fd=sd->fd;
	if(n<0 || n>=MAX_CART || sd->status.cart[n].nameid<=0)
		return 1;

#if PACKETVER < 5
	WFIFOHEAD(fd,packet_len(0x124));
	buf=WFIFOP(fd,0);
	WBUFW(buf,0)=0x124;
	WBUFW(buf,2)=n+2;
	WBUFL(buf,4)=amount;
	if((view = itemdb_viewid(sd->status.cart[n].nameid)) > 0)
		WBUFW(buf,8)=view;
	else
		WBUFW(buf,8)=sd->status.cart[n].nameid;
	WBUFB(buf,10)=sd->status.cart[n].identify;
	WBUFB(buf,11)=sd->status.cart[n].attribute;
	WBUFB(buf,12)=sd->status.cart[n].refine;
	clif_addcards(WBUFP(buf,13), &sd->status.cart[n]);
	WFIFOSET(fd,packet_len(0x124));
#else
	WFIFOHEAD(fd,packet_len(0x1c5));
	buf=WFIFOP(fd,0);
	WBUFW(buf,0)=0x1c5;
	WBUFW(buf,2)=n+2;
	WBUFL(buf,4)=amount;
	if((view = itemdb_viewid(sd->status.cart[n].nameid)) > 0)
		WBUFW(buf,8)=view;
	else
		WBUFW(buf,8)=sd->status.cart[n].nameid;
	WBUFB(buf,10)=itemdb_type(sd->status.cart[n].nameid);
	WBUFB(buf,11)=sd->status.cart[n].identify;
	WBUFB(buf,12)=sd->status.cart[n].attribute;
	WBUFB(buf,13)=sd->status.cart[n].refine;
	clif_addcards(WBUFP(buf,14), &sd->status.cart[n]);
	WFIFOSET(fd,packet_len(0x1c5));
#endif

	return 0;
}

/*==========================================
 * �J�[�g����A�C�e���폜
 *------------------------------------------*/
int clif_cart_delitem(struct map_session_data *sd,int n,int amount)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;

	WFIFOHEAD(fd,packet_len(0x125));
	WFIFOW(fd,0)=0x125;
	WFIFOW(fd,2)=n+2;
	WFIFOL(fd,4)=amount;

	WFIFOSET(fd,packet_len(0x125));

	return 0;
}

/*==========================================
 * Opens the shop creation menu.
 * R 012d <num>.w
 * 'num' is the number of allowed item slots
 *------------------------------------------*/
void clif_openvendingreq(struct map_session_data* sd, int num)
{
	int fd;

	nullpo_retv(sd);

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0x12d));
	WFIFOW(fd,0) = 0x12d;
	WFIFOW(fd,2) = num;
	WFIFOSET(fd,packet_len(0x12d));
}

/*==========================================
 * Displays a vending board to target/area
 * R 0131 <ID>.l <message>.80B
 *------------------------------------------*/
void clif_showvendingboard(struct block_list* bl, const char* message, int fd)
{
	unsigned char buf[128];

	nullpo_retv(bl);

	WBUFW(buf,0) = 0x131;
	WBUFL(buf,2) = bl->id;
	safestrncpy((char*)WBUFP(buf,6), message, 80);

	if( fd ) {
		WFIFOHEAD(fd,packet_len(0x131));
		memcpy(WFIFOP(fd,0),buf,packet_len(0x131));
		WFIFOSET(fd,packet_len(0x131));
	} else {
		clif_send(buf,packet_len(0x131),bl,AREA_WOS);
	}
}

/*==========================================
 * Removes a vending board from screen
 *------------------------------------------*/
void clif_closevendingboard(struct block_list* bl, int fd)
{
	unsigned char buf[16];

	nullpo_retv(bl);

	WBUFW(buf,0) = 0x132;
	WBUFL(buf,2) = bl->id;
	if( fd ) {
		WFIFOHEAD(fd,packet_len(0x132));
		memcpy(WFIFOP(fd,0),buf,packet_len(0x132));
		WFIFOSET(fd,packet_len(0x132));
	} else {
		clif_send(buf,packet_len(0x132),bl,AREA_WOS);
	}
}

/*==========================================
 * Sends a list of items in a shop (ZC_PC_PURCHASE_ITEMLIST_FROMMC/ZC_PC_PURCHASE_ITEMLIST_FROMMC2)
 * R 0133 <len>.w <ID>.l {<value>.l <amount>.w <index>.w <type>.B <item ID>.w <identify flag>.B <attribute?>.B <refine>.B <card>.4w}.22B
 * R 0800 <len>.w <ID>.l <UniqueID>.l {<value>.l  <amount>.w <index>.w <type>.B <item ID>.w <identify flag>.B <attribute?>.B <refine>.B <card>.4w}.22B
 *------------------------------------------*/
void clif_vendinglist(struct map_session_data* sd, int id, struct s_vending* vending)
{
	int i,fd;
	int count;
	struct map_session_data* vsd;
#if PACKETVER < 20100105
	const int cmd = 0x133;
	const int offset = 8;
#else
	const int cmd = 0x800;
	const int offset = 12;
#endif

	nullpo_retv(sd);
	nullpo_retv(vending);
	nullpo_retv(vsd=map_id2sd(id));

	fd = sd->fd;
	count = vsd->vend_num;

	WFIFOHEAD(fd, offset+count*22);
	WFIFOW(fd,0) = cmd;
	WFIFOW(fd,2) = offset+count*22;
	WFIFOL(fd,4) = id;
#if PACKETVER >= 20100105
	WFIFOL(fd,8) = vsd->vender_id;
#endif

	for( i = 0; i < count; i++ )
	{
		int index = vending[i].index;
		struct item_data* data = itemdb_search(vsd->status.cart[index].nameid);
		WFIFOL(fd,offset+ 0+i*22) = vending[i].value;
		WFIFOW(fd,offset+ 4+i*22) = vending[i].amount;
		WFIFOW(fd,offset+ 6+i*22) = vending[i].index + 2;
		WFIFOB(fd,offset+ 8+i*22) = itemtype(data->type);
		WFIFOW(fd,offset+ 9+i*22) = ( data->view_id > 0 ) ? data->view_id : vsd->status.cart[index].nameid;
		WFIFOB(fd,offset+11+i*22) = vsd->status.cart[index].identify;
		WFIFOB(fd,offset+12+i*22) = vsd->status.cart[index].attribute;
		WFIFOB(fd,offset+13+i*22) = vsd->status.cart[index].refine;
		clif_addcards(WFIFOP(fd,offset+14+i*22), &vsd->status.cart[index]);
	}
	WFIFOSET(fd,WFIFOW(fd,2));		
}

/*==========================================
 * Shop purchase failure (ZC_PC_PURCHASE_RESULT_FROMMC)
 * R 0135 <index>.w <amount>.w <fail>.B
 * fail=1 - not enough zeny
 * fail=2 - overweight
 * fail=4 - out of stock
 * fail=5 - "cannot use an npc shop while in a trade"
 * fail=6 - Because the store information was incorrect the item was not purchased.
 * fail=7 - No sales information.
 *------------------------------------------*/
void clif_buyvending(struct map_session_data* sd, int index, int amount, int fail)
{
	int fd;

	nullpo_retv(sd);

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0x135));
	WFIFOW(fd,0) = 0x135;
	WFIFOW(fd,2) = index+2;
	WFIFOW(fd,4) = amount;
	WFIFOB(fd,6) = fail;
	WFIFOSET(fd,packet_len(0x135));
}

/*==========================================
 * Shop creation success
 * R 0136 <len>.w <ID>.l {<value>.l <index>.w <amount>.w <type>.B <item ID>.w <identify flag>.B <attribute?>.B <refine>.B <card>.4w}.22B*
 *------------------------------------------*/
void clif_openvending(struct map_session_data* sd, int id, struct s_vending* vending)
{
	int i,fd;
	int count;

	nullpo_retv(sd);

	fd = sd->fd;
	count = sd->vend_num;

	WFIFOHEAD(fd, 8+count*22);
	WFIFOW(fd,0) = 0x136;
	WFIFOW(fd,2) = 8+count*22;
	WFIFOL(fd,4) = id;
	for( i = 0; i < count; i++ )
	{
		int index = vending[i].index;
		struct item_data* data = itemdb_search(sd->status.cart[index].nameid);
		WFIFOL(fd, 8+i*22) = vending[i].value;
		WFIFOW(fd,12+i*22) = vending[i].index + 2;
		WFIFOW(fd,14+i*22) = vending[i].amount;
		WFIFOB(fd,16+i*22) = itemtype(data->type);
		WFIFOW(fd,17+i*22) = ( data->view_id > 0 ) ? data->view_id : sd->status.cart[index].nameid;
		WFIFOB(fd,19+i*22) = sd->status.cart[index].identify;
		WFIFOB(fd,20+i*22) = sd->status.cart[index].attribute;
		WFIFOB(fd,21+i*22) = sd->status.cart[index].refine;
		clif_addcards(WFIFOP(fd,22+i*22), &sd->status.cart[index]);
	}
	WFIFOSET(fd,WFIFOW(fd,2));
}

/*==========================================
 * Inform merchant that someone has bought an item.
 * R 0137 <index>.w <amount>.w
 *------------------------------------------*/
void clif_vendingreport(struct map_session_data* sd, int index, int amount)
{
	int fd;

	nullpo_retv(sd);

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0x137));
	WFIFOW(fd,0) = 0x137;
	WFIFOW(fd,2) = index+2;
	WFIFOW(fd,4) = amount;
	WFIFOSET(fd,packet_len(0x137));
}

/// Result of organizing a party.
/// R 00FA <result>.B
///
/// result=0 : opens party window and shows MsgStringTable[77]="party successfully organized"
/// result=1 : MsgStringTable[78]="party name already exists"
/// result=2 : MsgStringTable[79]="already in a party"
/// result=other : nothing
int clif_party_created(struct map_session_data *sd,int result)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0xfa));
	WFIFOW(fd,0)=0xfa;
	WFIFOB(fd,2)=result;
	WFIFOSET(fd,packet_len(0xfa));
	return 0;
}

int clif_party_member_info(struct party_data *p, struct map_session_data *sd)
{
	unsigned char buf[96];
	int i;

	if (!sd) { //Pick any party member (this call is used when changing item share rules)
		ARR_FIND( 0, MAX_PARTY, i, p->data[i].sd != 0 );
	} else {
		ARR_FIND( 0, MAX_PARTY, i, p->data[i].sd == sd );
	}
	if (i >= MAX_PARTY) return 0; //Should never happen...
	sd = p->data[i].sd;

	WBUFW(buf, 0) = 0x1e9;
	WBUFL(buf, 2) = sd->status.account_id;
	WBUFL(buf, 6) = (p->party.member[i].leader)?0:1;
	WBUFW(buf,10) = sd->bl.x;
	WBUFW(buf,12) = sd->bl.y;
	WBUFB(buf,14) = (p->party.member[i].online)?0:1;
	memcpy(WBUFP(buf,15), p->party.name, NAME_LENGTH);
	memcpy(WBUFP(buf,39), sd->status.name, NAME_LENGTH);
	mapindex_getmapname_ext(mapindex_id2name(sd->mapindex), (char*)WBUFP(buf,63));
	WBUFB(buf,79) = (p->party.item&1)?1:0;
	WBUFB(buf,80) = (p->party.item&2)?1:0;
	clif_send(buf,packet_len(0x1e9),&sd->bl,PARTY);
	return 1;
}


/*==========================================
 * Sends party information
 * R 00fb <len>.w <party name>.24B {<ID>.l <nick>.24B <map name>.16B <leader>.B <offline>.B}.46B*
 *------------------------------------------*/
int clif_party_info(struct party_data* p, struct map_session_data *sd)
{
	unsigned char buf[2+2+NAME_LENGTH+(4+NAME_LENGTH+MAP_NAME_LENGTH_EXT+1+1)*MAX_PARTY];
	struct map_session_data* party_sd = NULL;
	int i, c;

	nullpo_ret(p);

	WBUFW(buf,0) = 0xfb;
	memcpy(WBUFP(buf,4), p->party.name, NAME_LENGTH);
	for(i = 0, c = 0; i < MAX_PARTY; i++)
	{
		struct party_member* m = &p->party.member[i];
		if(!m->account_id) continue;

		if(party_sd == NULL) party_sd = p->data[i].sd;

		WBUFL(buf,28+c*46) = m->account_id;
		memcpy(WBUFP(buf,28+c*46+4), m->name, NAME_LENGTH);
		mapindex_getmapname_ext(mapindex_id2name(m->map), (char*)WBUFP(buf,28+c*46+28));
		WBUFB(buf,28+c*46+44) = (m->leader) ? 0 : 1;
		WBUFB(buf,28+c*46+45) = (m->online) ? 0 : 1;
		c++;
	}
	WBUFW(buf,2) = 28+c*46;

	if(sd) { // send only to self
		clif_send(buf, WBUFW(buf,2), &sd->bl, SELF);
	} else if (party_sd) { // send to whole party
		clif_send(buf, WBUFW(buf,2), &party_sd->bl, PARTY);
	}
	
	return 0;
}

/*==========================================
 * The player's 'party invite' state, sent during login
 * R 02c9 <flag>.B
 *------------------------------------------*/
void clif_partyinvitationstate(struct map_session_data* sd)
{
	int fd;
	nullpo_retv(sd);
	fd = sd->fd;

	WFIFOHEAD(fd, packet_len(0x2c9));
	WFIFOW(fd, 0) = 0x2c9;
	WFIFOB(fd, 2) = 0; // not implemented
	WFIFOSET(fd, packet_len(0x2c9));
}

/// Party invitation request (ZC_REQ_JOIN_GROUP/ZC_PARTY_JOIN_REQ)
/// 00fe <party id>.L <party name>.24B
/// 02c6 <party id>.L <party name>.24B
void clif_party_invite(struct map_session_data *sd,struct map_session_data *tsd)
{
#if PACKETVER < 20070821
	const int cmd = 0xfe;
#else
	const int cmd = 0x2c6;
#endif
	int fd;
	struct party_data *p;

	nullpo_retv(sd);
	nullpo_retv(tsd);

	fd=tsd->fd;

	if( (p=party_search(sd->status.party_id))==NULL )
		return;

	WFIFOHEAD(fd,packet_len(cmd));
	WFIFOW(fd,0)=cmd;
	WFIFOL(fd,2)=sd->status.party_id;
	memcpy(WFIFOP(fd,6),p->party.name,NAME_LENGTH);
	WFIFOSET(fd,packet_len(cmd));
}


/// Party invite result.
/// R 00fd <nick>.24S <result>.B
/// R 02c5 <nick>.24S <result>.L
/// result=0 : char is already in a party -> MsgStringTable[80]
/// result=1 : party invite was rejected -> MsgStringTable[81]
/// result=2 : party invite was accepted -> MsgStringTable[82]
/// result=3 : party is full -> MsgStringTable[83]
/// result=4 : char of the same account already joined the party -> MsgStringTable[608]
/// result=5 : char blocked party invite -> MsgStringTable[1324] (since 20070904)
/// result=7 : char is not online or doesn't exist -> MsgStringTable[71] (since 20070904)
/// result=8 : (%s) TODO instance related? -> MsgStringTable[1388] (since 20080527)
/// return=9 : TODO map prohibits party joining? -> MsgStringTable[1871] (since 20110205)
void clif_party_inviteack(struct map_session_data* sd, const char* nick, int result)
{
	int fd;
	nullpo_retv(sd);
	fd=sd->fd;

#if PACKETVER < 20070904
	if( result == 7 ) {
		clif_displaymessage(fd, msg_txt(3));
		return;
	}
#endif

#if PACKETVER < 20070821
	WFIFOHEAD(fd,packet_len(0xfd));
	WFIFOW(fd,0) = 0xfd;
	safestrncpy((char*)WFIFOP(fd,2),nick,NAME_LENGTH);
	WFIFOB(fd,26) = result;
	WFIFOSET(fd,packet_len(0xfd));
#else
	WFIFOHEAD(fd,packet_len(0x2c5));
	WFIFOW(fd,0) = 0x2c5;
	safestrncpy((char*)WFIFOP(fd,2),nick,NAME_LENGTH);
	WFIFOL(fd,26) = result;
	WFIFOSET(fd,packet_len(0x2c5));
#endif
}


/*==========================================
 * �p�[�e�B�ݒ著�M
 * flag & 0x001=exp�ύX�~�X
 *        0x010=item�ύX�~�X
 *        0x100=��l�ɂ̂ݑ��M
 *------------------------------------------*/
int clif_party_option(struct party_data *p,struct map_session_data *sd,int flag)
{
	unsigned char buf[16];
#if PACKETVER<20090603
	const int cmd = 0x101;
#else
	const int cmd = 0x7d8;
#endif

	nullpo_ret(p);

	if(!sd && flag==0){
		int i;
		for(i=0;i<MAX_PARTY && !p->data[i].sd;i++);
		if (i < MAX_PARTY)
			sd = p->data[i].sd;
	}
	if(!sd) return 0;
	WBUFW(buf,0)=cmd;
	// WBUFL(buf,2) // that's how the client reads it, still need to check it's uses [FlavioJS]
	WBUFW(buf,2)=((flag&0x01)?2:p->party.exp);
	WBUFW(buf,4)=0;
#if PACKETVER>=20090603
	WBUFB(buf,6)=(p->party.item&1)?1:0;
	WBUFB(buf,7)=(p->party.item&2)?1:0;
#endif
	if(flag==0)
		clif_send(buf,packet_len(cmd),&sd->bl,PARTY);
	else
		clif_send(buf,packet_len(cmd),&sd->bl,SELF);
	return 0;
}
/*==========================================
 * �p�[�e�B�E�ށi�E�ޑO�ɌĂԂ��Ɓj
 *------------------------------------------*/
int clif_party_withdraw(struct party_data* p, struct map_session_data* sd, int account_id, const char* name, int flag)
{
	unsigned char buf[64];
	int i;

	nullpo_ret(p);

	if(!sd && (flag&0xf0)==0)
	{
		for(i=0;i<MAX_PARTY && !p->data[i].sd;i++);
			if (i < MAX_PARTY)
				sd = p->data[i].sd;
	}

	if(!sd) return 0;

	WBUFW(buf,0)=0x105;
	WBUFL(buf,2)=account_id;
	memcpy(WBUFP(buf,6),name,NAME_LENGTH);
	WBUFB(buf,30)=flag&0x0f;

	if((flag&0xf0)==0)
		clif_send(buf,packet_len(0x105),&sd->bl,PARTY);
	 else
		clif_send(buf,packet_len(0x105),&sd->bl,SELF);
	return 0;
}
/*==========================================
 * �p�[�e�B���b�Z�[�W���M
 *------------------------------------------*/
int clif_party_message(struct party_data* p, int account_id, const char* mes, int len)
{
	struct map_session_data *sd;
	int i;

	nullpo_ret(p);

	for(i=0; i < MAX_PARTY && !p->data[i].sd;i++);
	if(i < MAX_PARTY){
		unsigned char buf[1024];
		sd = p->data[i].sd;
		WBUFW(buf,0)=0x109;
		WBUFW(buf,2)=len+8;
		WBUFL(buf,4)=account_id;
		memcpy(WBUFP(buf,8),mes,len);
		clif_send(buf,len+8,&sd->bl,PARTY);
	}
	return 0;
}
/*==========================================
 * �p�[�e�B���W�ʒm
 *------------------------------------------*/
int clif_party_xy(struct map_session_data *sd)
{
	unsigned char buf[16];

	nullpo_ret(sd);

	WBUFW(buf,0)=0x107;
	WBUFL(buf,2)=sd->status.account_id;
	WBUFW(buf,6)=sd->bl.x;
	WBUFW(buf,8)=sd->bl.y;
	clif_send(buf,packet_len(0x107),&sd->bl,PARTY_SAMEMAP_WOS);
	
	return 0;
}

/*==========================================
 * Sends x/y dot to a single fd. [Skotlex]
 *------------------------------------------*/
int clif_party_xy_single(int fd, struct map_session_data *sd)
{
	WFIFOHEAD(fd,packet_len(0x107));
	WFIFOW(fd,0)=0x107;
	WFIFOL(fd,2)=sd->status.account_id;
	WFIFOW(fd,6)=sd->bl.x;
	WFIFOW(fd,8)=sd->bl.y;
	WFIFOSET(fd,packet_len(0x107));
	return 0;
}


/*==========================================
 * �p�[�e�BHP�ʒm
 *------------------------------------------*/
int clif_party_hp(struct map_session_data *sd)
{
	unsigned char buf[16];
#if PACKETVER < 20100126
	const int cmd = 0x106;
#else
	const int cmd = 0x80e;
#endif

	nullpo_ret(sd);

	WBUFW(buf,0)=cmd;
	WBUFL(buf,2)=sd->status.account_id;
#if PACKETVER < 20100126
	if (sd->battle_status.max_hp > INT16_MAX) { //To correctly display the %hp bar. [Skotlex]
		WBUFW(buf,6) = sd->battle_status.hp/(sd->battle_status.max_hp/100);
		WBUFW(buf,8) = 100;
	} else {
		WBUFW(buf,6) = sd->battle_status.hp;
		WBUFW(buf,8) = sd->battle_status.max_hp;
	}
#else
	WBUFL(buf,6) = sd->battle_status.hp;
	WBUFL(buf,10) = sd->battle_status.max_hp;
#endif
	clif_send(buf,packet_len(cmd),&sd->bl,PARTY_AREA_WOS);
	return 0;
}

/*==========================================
 * Sends HP bar to a single fd. [Skotlex]
 *------------------------------------------*/
void clif_hpmeter_single(int fd, int id, unsigned int hp, unsigned int maxhp)
{
#if PACKETVER < 20100126
	const int cmd = 0x106;
#else
	const int cmd = 0x80e;
#endif
	WFIFOHEAD(fd,packet_len(cmd));
	WFIFOW(fd,0) = cmd;
	WFIFOL(fd,2) = id;
#if PACKETVER < 20100126
	if( maxhp > INT16_MAX )
	{// To correctly display the %hp bar. [Skotlex]
		WFIFOW(fd,6) = hp/(maxhp/100);
		WFIFOW(fd,8) = 100;
	} else {
		WFIFOW(fd,6) = hp;
		WFIFOW(fd,8) = maxhp;
	}
#else
	WFIFOL(fd,6) = hp;
	WFIFOL(fd,10) = maxhp;
#endif
	WFIFOSET(fd, packet_len(cmd));
}

int clif_mobhpmeter(struct mob_data *md)
{
	unsigned char buf[16];
#if PACKETVER < 20100414
	const int cmd = 0x106;
#else
	const int cmd = 0x80e;
#endif
	nullpo_ret(md);

	WBUFW(buf,0)=cmd;
	WBUFL(buf,2) = md->bl.id;
#if PACKETVER < 20100414
	if (md->status.max_hp > INT16_MAX) {
		WBUFW(buf,6) = md->status.hp/(md->status.max_hp/100);
		WBUFW(buf,8) = 100;
	} else {
		WBUFW(buf,6) = md->status.hp;
		WBUFW(buf,8) = md->status.max_hp;
	}
#else
	WBUFL(buf,6) = md->status.hp;
	WBUFL(buf,10) = md->status.max_hp;
#endif

	clif_send(buf, packet_len(cmd), &md->bl, AREA);
	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_hpmeter_sub(struct block_list *bl, va_list ap)
{
	struct map_session_data *sd, *tsd;
	int level;
#if PACKETVER < 20100126
	const int cmd = 0x106;
#else
	const int cmd = 0x80e;
#endif

	sd = va_arg(ap, struct map_session_data *);
	tsd = (TBL_PC *)bl;

	nullpo_ret(sd);
	nullpo_ret(tsd);

	if( !tsd->fd || tsd == sd )
		return 0;

	if( (level = pc_isGM(tsd)) < battle_config.disp_hpmeter || level < pc_isGM(sd) )
		return 0;
	WFIFOHEAD(tsd->fd,packet_len(cmd));
	WFIFOW(tsd->fd,0) = cmd;
	WFIFOL(tsd->fd,2) = sd->status.account_id;
#if PACKETVER < 20100126
	if( sd->battle_status.max_hp > INT16_MAX )
	{ //To correctly display the %hp bar. [Skotlex]
		WFIFOW(tsd->fd,6) = sd->battle_status.hp/(sd->battle_status.max_hp/100);
		WFIFOW(tsd->fd,8) = 100;
	} else {
		WFIFOW(tsd->fd,6) = sd->battle_status.hp;
		WFIFOW(tsd->fd,8) = sd->battle_status.max_hp;
	}
#else
	WFIFOL(tsd->fd,6) = sd->battle_status.hp;
	WFIFOL(tsd->fd,10) = sd->battle_status.max_hp;
#endif
	WFIFOSET(tsd->fd,packet_len(cmd));
	return 0;
}

/*==========================================
 * GM�֏ꏊ��HP�ʒm
 *------------------------------------------*/
int clif_hpmeter(struct map_session_data *sd)
{
	nullpo_ret(sd);

	if( battle_config.disp_hpmeter )
		map_foreachinarea(clif_hpmeter_sub, sd->bl.m, sd->bl.x-AREA_SIZE, sd->bl.y-AREA_SIZE, sd->bl.x+AREA_SIZE, sd->bl.y+AREA_SIZE, BL_PC, sd);

	return 0;
}

/*==========================================
 * �p�[�e�B�ꏊ�ړ��i���g�p�j
 *------------------------------------------*/
void clif_party_move(struct party* p, struct map_session_data* sd, int online)
{
	unsigned char buf[128];

	nullpo_retv(sd);
	nullpo_retv(p);

	WBUFW(buf, 0) = 0x104;
	WBUFL(buf, 2) = sd->status.account_id;
	WBUFL(buf, 6) = 0;
	WBUFW(buf,10) = sd->bl.x;
	WBUFW(buf,12) = sd->bl.y;
	WBUFB(buf,14) = !online;
	memcpy(WBUFP(buf,15),p->name, NAME_LENGTH);
	memcpy(WBUFP(buf,39),sd->status.name, NAME_LENGTH);
	mapindex_getmapname_ext(map[sd->bl.m].name, (char*)WBUFP(buf,63));
	clif_send(buf,packet_len(0x104),&sd->bl,PARTY);
}
/*==========================================
 * �U�����邽�߂Ɉړ����K�v
 *------------------------------------------*/
int clif_movetoattack(struct map_session_data *sd,struct block_list *bl)
{
	int fd;

	nullpo_ret(sd);
	nullpo_ret(bl);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x139));
	WFIFOW(fd, 0)=0x139;
	WFIFOL(fd, 2)=bl->id;
	WFIFOW(fd, 6)=bl->x;
	WFIFOW(fd, 8)=bl->y;
	WFIFOW(fd,10)=sd->bl.x;
	WFIFOW(fd,12)=sd->bl.y;
	WFIFOW(fd,14)=sd->battle_status.rhw.range;
	WFIFOSET(fd,packet_len(0x139));
	return 0;
}
/*==========================================
 * �����G�t�F�N�g
 *------------------------------------------*/
int clif_produceeffect(struct map_session_data* sd,int flag,int nameid)
{
	int view,fd;

	nullpo_ret(sd);

	fd = sd->fd;
	clif_solved_charname(fd, sd->status.char_id, sd->status.name);
	WFIFOHEAD(fd,packet_len(0x18f));
	WFIFOW(fd, 0)=0x18f;
	WFIFOW(fd, 2)=flag;
	if((view = itemdb_viewid(nameid)) > 0)
		WFIFOW(fd, 4)=view;
	else
		WFIFOW(fd, 4)=nameid;
	WFIFOSET(fd,packet_len(0x18f));
	return 0;
}

// pet
int clif_catch_process(struct map_session_data *sd)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x19e));
	WFIFOW(fd,0)=0x19e;
	WFIFOSET(fd,packet_len(0x19e));
	return 0;
}

int clif_pet_roulette(struct map_session_data *sd,int data)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x1a0));
	WFIFOW(fd,0)=0x1a0;
	WFIFOB(fd,2)=data;
	WFIFOSET(fd,packet_len(0x1a0));

	return 0;
}

/*==========================================
 * pet�����X�g�쐬
 *------------------------------------------*/
int clif_sendegg(struct map_session_data *sd)
{
	//R 01a6 <len>.w <index>.w*
	int i,n=0,fd;

	nullpo_ret(sd);

	fd=sd->fd;
	if (battle_config.pet_no_gvg && map_flag_gvg(sd->bl.m))
	{	//Disable pet hatching in GvG grounds during Guild Wars [Skotlex]
		clif_displaymessage(fd, "Pets are not allowed in Guild Wars.");
		return 0;
	}
	if( map[sd->bl.m].flag.ancient )
	{
		clif_displaymessage(fd, "Pets are not allowed in Ancient WoE.");
		return 0;
	}
	if( sd->sc.data[SC__GROOMY] )
		return 0;

	WFIFOHEAD(fd, MAX_INVENTORY * 2 + 4);
	WFIFOW(fd,0)=0x1a6;
	for(i=0,n=0;i<MAX_INVENTORY;i++){
		if(sd->status.inventory[i].nameid<=0 || sd->inventory_data[i] == NULL ||
		   sd->inventory_data[i]->type!=IT_PETEGG ||
		   sd->status.inventory[i].amount<=0)
			continue;
		WFIFOW(fd,n*2+4)=i+2;
		n++;
	}
	WFIFOW(fd,2)=4+n*2;
	WFIFOSET(fd,WFIFOW(fd,2));

	sd->menuskill_id = SA_TAMINGMONSTER;
	sd->menuskill_val = -1;
	return 0;
}

/*==========================================
 * Sends a specific pet data update.
 * type = 0 -> param = 0 (initial data)
 * type = 1 -> param = intimacy value
 * type = 2 -> param = hungry value
 * type = 3 -> param = accessory id
 * type = 4 -> param = performance number (1-3:normal, 4:special)
 * type = 5 -> param = hairstyle number
 * If sd is null, the update is sent to nearby objects, otherwise it is sent only to that player.
 *------------------------------------------*/
int clif_send_petdata(struct map_session_data* sd, struct pet_data* pd, int type, int param)
{
	uint8 buf[16];
	nullpo_ret(pd);

	WBUFW(buf,0) = 0x1a4;
	WBUFB(buf,2) = type;
	WBUFL(buf,3) = pd->bl.id;
	WBUFL(buf,7) = param;
	if (sd)
		clif_send(buf, packet_len(0x1a4), &sd->bl, SELF);
	else
		clif_send(buf, packet_len(0x1a4), &pd->bl, AREA);
	return 0;
}

int clif_send_petstatus(struct map_session_data *sd)
{
	int fd;
	struct s_pet *pet;

	nullpo_ret(sd);
	nullpo_ret(sd->pd);

	fd=sd->fd;
	pet = &sd->pd->pet;
	WFIFOHEAD(fd,packet_len(0x1a2));
	WFIFOW(fd,0)=0x1a2;
	memcpy(WFIFOP(fd,2),pet->name,NAME_LENGTH);
	WFIFOB(fd,26)=battle_config.pet_rename?0:pet->rename_flag;
	WFIFOW(fd,27)=pet->level;
	WFIFOW(fd,29)=pet->hungry;
	WFIFOW(fd,31)=pet->intimate;
	WFIFOW(fd,33)=pet->equip;
#if PACKETVER >= 20081126
	WFIFOW(fd,35)=pet->class_;
#endif
	WFIFOSET(fd,packet_len(0x1a2));

	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_pet_emotion(struct pet_data *pd,int param)
{
	unsigned char buf[16];

	nullpo_ret(pd);

	memset(buf,0,packet_len(0x1aa));

	WBUFW(buf,0)=0x1aa;
	WBUFL(buf,2)=pd->bl.id;
	if(param >= 100 && pd->petDB->talk_convert_class) {
		if(pd->petDB->talk_convert_class < 0)
			return 0;
		else if(pd->petDB->talk_convert_class > 0) {
			param -= (pd->pet.class_ - 100)*100;
			param += (pd->petDB->talk_convert_class - 100)*100;
		}
	}
	WBUFL(buf,6)=param;

	clif_send(buf,packet_len(0x1aa),&pd->bl,AREA);

	return 0;
}

int clif_pet_food(struct map_session_data *sd,int foodid,int fail)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x1a3));
	WFIFOW(fd,0)=0x1a3;
	WFIFOB(fd,2)=fail;
	WFIFOW(fd,3)=foodid;
	WFIFOSET(fd,packet_len(0x1a3));

	return 0;
}

/*==========================================
 * �I�[�g�X�y�� ���X�g���M
 *------------------------------------------*/
int clif_autospell(struct map_session_data *sd,int skilllv)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x1cd));
	WFIFOW(fd, 0)=0x1cd;

	if(skilllv>0 && pc_checkskill(sd,MG_NAPALMBEAT)>0)
		WFIFOL(fd,2)= MG_NAPALMBEAT;
	else
		WFIFOL(fd,2)= 0x00000000;
	if(skilllv>1 && pc_checkskill(sd,MG_COLDBOLT)>0)
		WFIFOL(fd,6)= MG_COLDBOLT;
	else
		WFIFOL(fd,6)= 0x00000000;
	if(skilllv>1 && pc_checkskill(sd,MG_FIREBOLT)>0)
		WFIFOL(fd,10)= MG_FIREBOLT;
	else
		WFIFOL(fd,10)= 0x00000000;
	if(skilllv>1 && pc_checkskill(sd,MG_LIGHTNINGBOLT)>0)
		WFIFOL(fd,14)= MG_LIGHTNINGBOLT;
	else
		WFIFOL(fd,14)= 0x00000000;
	if(skilllv>4 && pc_checkskill(sd,MG_SOULSTRIKE)>0)
		WFIFOL(fd,18)= MG_SOULSTRIKE;
	else
		WFIFOL(fd,18)= 0x00000000;
	if(skilllv>7 && pc_checkskill(sd,MG_FIREBALL)>0)
		WFIFOL(fd,22)= MG_FIREBALL;
	else
		WFIFOL(fd,22)= 0x00000000;
	if(skilllv>9 && pc_checkskill(sd,MG_FROSTDIVER)>0)
		WFIFOL(fd,26)= MG_FROSTDIVER;
	else
		WFIFOL(fd,26)= 0x00000000;

	WFIFOSET(fd,packet_len(0x1cd));
	sd->menuskill_id = SA_AUTOSPELL;
	sd->menuskill_val = skilllv;
	
	return 0;
}

/*==========================================
 * Devotion's visual effect
 * S 01cf <devoter id>.L { <devotee id>.L }[5] <max distance>.W
 *------------------------------------------*/
void clif_devotion(struct block_list *src, struct map_session_data *tsd)
{
	unsigned char buf[56];
	int i;
	
	nullpo_retv(src);
	memset(buf,0,packet_len(0x1cf));

	WBUFW(buf,0) = 0x1cf;
	WBUFL(buf,2) = src->id;
	if( src->type == BL_MER )
	{
		struct mercenary_data *md = BL_CAST(BL_MER,src);
		if( md && md->master && md->devotion_flag )
			WBUFL(buf,6) = md->master->bl.id;

		WBUFW(buf,26) = skill_get_range2(src, ML_DEVOTION, mercenary_checkskill(md, ML_DEVOTION));
	}
	else
	{
		struct map_session_data *sd = BL_CAST(BL_PC,src);
		if( sd == NULL )
			return;

		for( i = 0; i < 5; i++ )
			WBUFL(buf,6+4*i) = sd->devotion[i];
		WBUFW(buf,26) = skill_get_range2(src, CR_DEVOTION, pc_checkskill(sd, CR_DEVOTION));
	}

	if( tsd )
		clif_send(buf, packet_len(0x1cf), &tsd->bl, SELF);
	else
		clif_send(buf, packet_len(0x1cf), src, AREA);
}

/*==========================================
 * ����
 *------------------------------------------*/
int clif_spiritball(struct map_session_data *sd)
{
	unsigned char buf[16];

	nullpo_ret(sd);

	WBUFW(buf,0)=0x1d0;
	WBUFL(buf,2)=sd->bl.id;
	WBUFW(buf,6)=sd->spiritball;
	clif_send(buf,packet_len(0x1d0),&sd->bl,AREA);
	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int clif_combo_delay(struct block_list *bl,int wait)
{
	unsigned char buf[32];

	nullpo_ret(bl);

	WBUFW(buf,0)=0x1d2;
	WBUFL(buf,2)=bl->id;
	WBUFL(buf,6)=wait;
	clif_send(buf,packet_len(0x1d2),bl,AREA);

	return 0;
}
/*==========================================
 *���n���
 *------------------------------------------*/
void clif_bladestop(struct block_list *src, int dst_id, int active)
{
	unsigned char buf[32];

	nullpo_retv(src);

	WBUFW(buf,0)=0x1d1;
	WBUFL(buf,2)=src->id;
	WBUFL(buf,6)=dst_id;
	WBUFL(buf,10)=active;

	clif_send(buf,packet_len(0x1d1),src,AREA);
}

/*==========================================
 * MVP�G�t�F�N�g
 *------------------------------------------*/
int clif_mvp_effect(struct map_session_data *sd)
{
	unsigned char buf[16];

	nullpo_ret(sd);

	WBUFW(buf,0)=0x10c;
	WBUFL(buf,2)=sd->bl.id;
	clif_send(buf,packet_len(0x10c),&sd->bl,AREA);
	return 0;
}
/*==========================================
 * MVP�A�C�e������
 *------------------------------------------*/
int clif_mvp_item(struct map_session_data *sd,int nameid)
{
	int view,fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x10a));
	WFIFOW(fd,0)=0x10a;
	if((view = itemdb_viewid(nameid)) > 0)
		WFIFOW(fd,2)=view;
	else
		WFIFOW(fd,2)=nameid;
	WFIFOSET(fd,packet_len(0x10a));
	return 0;
}
/*==========================================
 * MVP�o���l����
 *------------------------------------------*/
int clif_mvp_exp(struct map_session_data *sd, unsigned int exp)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x10b));
	WFIFOW(fd,0)=0x10b;
	WFIFOL(fd,2)=cap_value(exp,0,INT32_MAX);
	WFIFOSET(fd,packet_len(0x10b));
	return 0;
}

/*==========================================
 * Guild creation result
 * R 0167 <flag>.B
 * flag = 0 -> "Guild has been created."
 * flag = 1 -> "You are already in a Guild."
 * flag = 2 -> "That Guild Name already exists."
 * flag = 3 -> "You need the neccessary item to create a Guild."
 *------------------------------------------*/
int clif_guild_created(struct map_session_data *sd,int flag)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x167));
	WFIFOW(fd,0)=0x167;
	WFIFOB(fd,2)=flag;
	WFIFOSET(fd,packet_len(0x167));
	return 0;
}


/// Notifies the client that it is belonging to a guild (ZC_UPDATE_GDID)
/// 016c <guild id>.L <emblem id>.L <mode>.L <ismaster>.B <inter sid>.L <guild name>.24B
void clif_guild_belonginfo(struct map_session_data *sd, struct guild *g)
{
	int ps,fd;
	nullpo_retv(sd);
	if( battle_config.bg_eAmod_mode && sd->bg_id )
	{
		clif_bg_belonginfo(sd);
		return;
	}
	nullpo_retv(g);

	fd=sd->fd;
	ps=guild_getposition(g,sd);
	WFIFOHEAD(fd,packet_len(0x16c));
	WFIFOW(fd,0)=0x16c;
	WFIFOL(fd,2)=g->guild_id;
	WFIFOL(fd,6)=g->emblem_id;
	WFIFOL(fd,10)=g->position[ps].mode;
	WFIFOB(fd,14)=(bool)(sd->state.gmaster_flag==g);
	WFIFOL(fd,15)=0;  // InterSID (unknown purpose)
	memcpy(WFIFOP(fd,19),g->name,NAME_LENGTH);
	WFIFOSET(fd,packet_len(0x16c));
}


/*==========================================
 * �M���h�����o���O�C���ʒm
 *------------------------------------------*/
int clif_guild_memberlogin_notice(struct guild *g,int idx,int flag)
{
	unsigned char buf[64];

	nullpo_ret(g);

	WBUFW(buf, 0)=0x16d;
	WBUFL(buf, 2)=g->member[idx].account_id;
	WBUFL(buf, 6)=g->member[idx].char_id;
	WBUFL(buf,10)=flag;
	if(g->member[idx].sd==NULL){
		struct map_session_data *sd=guild_getavailablesd(g);
		if(sd!=NULL)
			clif_send(buf,packet_len(0x16d),&sd->bl,GUILD);
	}else
		clif_send(buf,packet_len(0x16d),&g->member[idx].sd->bl,GUILD_WOS);
	return 0;
}

// Function `clif_guild_memberlogin_notice` sends info about
// logins and logouts of a guild member to the rest members.
// But at the 1st time (after a player login or map changing)
// the client won't show the message.
// So I suggest use this function for sending "first-time-info"
// to some player on entering the game or changing location. 
// At next time the client would always show the message.
// The function sends all the statuses in the single packet 
// to economize traffic. [LuzZza]
int clif_guild_send_onlineinfo(struct map_session_data *sd)
{
	struct guild *g;
	unsigned char buf[14*128];
	int i, count=0, p_len;
	
	nullpo_ret(sd);

	p_len = packet_len(0x16d);

	if(!(g = guild_search(sd->status.guild_id)))
		return 0;
	
	for(i=0; i<g->max_member; i++) {

		if(g->member[i].account_id > 0 &&
			g->member[i].account_id != sd->status.account_id) {

			WBUFW(buf,count*p_len) = 0x16d;
			WBUFL(buf,count*p_len+2) = g->member[i].account_id;
			WBUFL(buf,count*p_len+6) = g->member[i].char_id;
			WBUFL(buf,count*p_len+10) = g->member[i].online;
			count++;
		}
	}
	
	clif_send(buf, p_len*count, &sd->bl, SELF);

	return 0;
}

/*==========================================
 * �M���h�}�X�^�[�ʒm(14d�ւ̉���)
 *------------------------------------------*/
int clif_guild_masterormember(struct map_session_data *sd)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x14e));
	WFIFOW(fd,0) = 0x14e;
	WFIFOL(fd,2) = (sd->state.gmaster_flag) ? 0xd7 : 0x57;
	WFIFOSET(fd,packet_len(0x14e));
	return 0;
}
/*==========================================
 * Basic Info (Territories [Valaris])
 *------------------------------------------*/
int clif_guild_basicinfo(struct map_session_data *sd)
{
	int fd,i,t;
	struct guild *g;
	struct guild_castle *gc = NULL;
	struct battleground_data *bg = NULL;

	nullpo_ret(sd);
	fd = sd->fd;

	if( battle_config.bg_eAmod_mode && sd->bg_id && (g = bg_guild_get(sd->bg_id)) != NULL )
		bg = bg_team_search(sd->bg_id);
	else
		g = guild_search(sd->status.guild_id);

	if( g == NULL )
		return 0;

	WFIFOHEAD(fd,packet_len(0x1b6));
	WFIFOW(fd, 0)=0x1b6;//0x150;
	WFIFOL(fd, 2)=g->guild_id;
	WFIFOL(fd, 6)=g->guild_lv;
	WFIFOL(fd,10)=bg?bg->count:g->connect_member;
	WFIFOL(fd,14)=g->max_member;
	WFIFOL(fd,18)=g->average_lv;
	WFIFOL(fd,22)=(uint32)cap_value(g->exp,0,INT32_MAX);
	WFIFOL(fd,26)=g->next_exp;
	WFIFOL(fd,30)=0;	// Tax Points
	WFIFOL(fd,34)=0;	// Tendency: (left) Vulgar [-100,100] Famed (right)
	WFIFOL(fd,38)=0;	// Tendency: (down) Wicked [-100,100] Righteous (up)
	WFIFOL(fd,42)=g->emblem_id;
	memcpy(WFIFOP(fd,46),g->name, NAME_LENGTH);
	memcpy(WFIFOP(fd,70),g->master, NAME_LENGTH);

	for(i = 0, t = 0; i < MAX_GUILDCASTLE; i++)
	{
		gc = guild_castle_search(i);
		if(gc && g->guild_id == gc->guild_id)
			t++;
	}
	strncpy((char*)WFIFOP(fd,94),msg_txt(300+t),20); // "'N' castles"

	WFIFOSET(fd,packet_len(WFIFOW(fd,0)));
	return 0;
}

/*==========================================
 * �M���h����/�G�Ώ��
 *------------------------------------------*/
int clif_guild_allianceinfo(struct map_session_data *sd)
{
	int fd,i,c;
	struct guild *g;

	nullpo_ret(sd);

	if( !(battle_config.bg_eAmod_mode && sd->bg_id && (g = bg_guild_get(sd->bg_id)) != NULL) )
		g = guild_search(sd->status.guild_id);

	if( g == NULL )
		return 0;

	fd = sd->fd;
	WFIFOHEAD(fd, MAX_GUILDALLIANCE * 32 + 4);
	WFIFOW(fd, 0)=0x14c;
	for(i=c=0;i<MAX_GUILDALLIANCE;i++){
		struct guild_alliance *a=&g->alliance[i];
		if(a->guild_id>0){
			WFIFOL(fd,c*32+4)=a->opposition;
			WFIFOL(fd,c*32+8)=a->guild_id;
			memcpy(WFIFOP(fd,c*32+12),a->name,NAME_LENGTH);
			c++;
		}
	}
	WFIFOW(fd, 2)=c*32+4;
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}

/*==========================================
 * �M���h�����o�[���X�g
 *------------------------------------------*/
int clif_guild_memberlist(struct map_session_data *sd)
{
	int fd;
	int i,c;
	struct guild *g;
	nullpo_ret(sd);

	if( (fd = sd->fd) == 0 )
		return 0;
	if( battle_config.bg_eAmod_mode && sd->bg_id )
		return clif_bg_memberlist(sd);
	if( (g = guild_search(sd->status.guild_id)) == NULL )
		return 0;

	WFIFOHEAD(fd, g->max_member * 104 + 4);
	WFIFOW(fd, 0)=0x154;
	for(i=0,c=0;i<g->max_member;i++){
		struct guild_member *m=&g->member[i];
		if(m->account_id==0)
			continue;
		WFIFOL(fd,c*104+ 4)=m->account_id;
		WFIFOL(fd,c*104+ 8)=m->char_id;
		WFIFOW(fd,c*104+12)=m->hair;
		WFIFOW(fd,c*104+14)=m->hair_color;
		WFIFOW(fd,c*104+16)=m->gender;
		WFIFOW(fd,c*104+18)=m->class_;
		WFIFOW(fd,c*104+20)=m->lv;
		WFIFOL(fd,c*104+22)=(int)cap_value(m->exp,0,INT32_MAX);
		WFIFOL(fd,c*104+26)=m->online;
		WFIFOL(fd,c*104+30)=m->position;
		memset(WFIFOP(fd,c*104+34),0,50);	// �����H
		memcpy(WFIFOP(fd,c*104+84),m->name,NAME_LENGTH);
		c++;
	}
	WFIFOW(fd, 2)=c*104+4;
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}
/*==========================================
 * �M���h��E�����X�g
 *------------------------------------------*/
int clif_guild_positionnamelist(struct map_session_data *sd)
{
	int i,fd;
	struct guild *g;

	nullpo_ret(sd);
	if( !(battle_config.bg_eAmod_mode && sd->bg_id && (g = bg_guild_get(sd->bg_id)) != NULL) )
		g = guild_search(sd->status.guild_id);

	if( g == NULL )
		return 0;

	fd = sd->fd;
	WFIFOHEAD(fd, MAX_GUILDPOSITION * 28 + 4);
	WFIFOW(fd, 0)=0x166;
	for(i=0;i<MAX_GUILDPOSITION;i++){
		WFIFOL(fd,i*28+4)=i;
		memcpy(WFIFOP(fd,i*28+8),g->position[i].name,NAME_LENGTH);
	}
	WFIFOW(fd,2)=i*28+4;
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}
/*==========================================
 * �M���h��E��񃊃X�g
 *------------------------------------------*/
int clif_guild_positioninfolist(struct map_session_data *sd)
{
	int i,fd;
	struct guild *g;

	nullpo_ret(sd);
	if( !(battle_config.bg_eAmod_mode && sd->bg_id && (g = bg_guild_get(sd->bg_id)) != NULL) )
		g = guild_search(sd->status.guild_id);

	if( g == NULL )
		return 0;

	fd = sd->fd;
	WFIFOHEAD(fd, MAX_GUILDPOSITION * 16 + 4);
	WFIFOW(fd, 0)=0x160;
	for(i=0;i<MAX_GUILDPOSITION;i++){
		struct guild_position *p=&g->position[i];
		WFIFOL(fd,i*16+ 4)=i;
		WFIFOL(fd,i*16+ 8)=p->mode;
		WFIFOL(fd,i*16+12)=i;
		WFIFOL(fd,i*16+16)=p->exp_mode;
	}
	WFIFOW(fd, 2)=i*16+4;
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}
/*==========================================
 * �M���h��E�ύX�ʒm
 *------------------------------------------*/
int clif_guild_positionchanged(struct guild *g,int idx)
{
	struct map_session_data *sd;
	unsigned char buf[128];

	nullpo_ret(g);

	WBUFW(buf, 0)=0x174;
	WBUFW(buf, 2)=44;
	WBUFL(buf, 4)=idx;
	WBUFL(buf, 8)=g->position[idx].mode;
	WBUFL(buf,12)=idx;
	WBUFL(buf,16)=g->position[idx].exp_mode;
	memcpy(WBUFP(buf,20),g->position[idx].name,NAME_LENGTH);
	if( (sd=guild_getavailablesd(g))!=NULL )
		clif_send(buf,WBUFW(buf,2),&sd->bl,GUILD);
	return 0;
}
/*==========================================
 * �M���h�����o�ύX�ʒm
 *------------------------------------------*/
int clif_guild_memberpositionchanged(struct guild *g,int idx)
{
	struct map_session_data *sd;
	unsigned char buf[64];

	nullpo_ret(g);

	WBUFW(buf, 0)=0x156;
	WBUFW(buf, 2)=16;
	WBUFL(buf, 4)=g->member[idx].account_id;
	WBUFL(buf, 8)=g->member[idx].char_id;
	WBUFL(buf,12)=g->member[idx].position;
	if( (sd=guild_getavailablesd(g))!=NULL )
		clif_send(buf,WBUFW(buf,2),&sd->bl,GUILD);
	return 0;
}
/*==========================================
 * �M���h�G���u�������M
 *------------------------------------------*/
int clif_guild_emblem(struct map_session_data *sd,struct guild *g)
{
	int fd;
	nullpo_ret(sd);
	nullpo_ret(g);

	fd = sd->fd;
	if( g->emblem_len <= 0 )
		return 0;

	WFIFOHEAD(fd,g->emblem_len+12);
	WFIFOW(fd,0)=0x152;
	WFIFOW(fd,2)=g->emblem_len+12;
	WFIFOL(fd,4)=g->guild_id;
	WFIFOL(fd,8)=g->emblem_id;
	memcpy(WFIFOP(fd,12),g->emblem_data,g->emblem_len);
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}

/// Sends update of the guild id/emblem id to everyone in the area.
void clif_guild_emblem_area(struct block_list* bl)
{
	uint8 buf[12];

	nullpo_retv(bl);

	// TODO this packet doesn't force the update of ui components that have the emblem visible
	//      (emblem in the flag npcs and emblem over the head in agit maps) [FlavioJS]
	WBUFW(buf,0) = 0x1B4;
	WBUFL(buf,2) = bl->id;
	WBUFL(buf,6) = clif_visual_guild_id(bl);
	WBUFW(buf,10) = clif_visual_emblem_id(bl);
	clif_send(buf, 12, bl, AREA_WOS);
}

/*==========================================
 * Send guild skills
 *------------------------------------------*/
int clif_guild_skillinfo(struct map_session_data* sd)
{
	int fd;
	struct guild* g;
	int i,c;

	nullpo_ret(sd);
	if( !(battle_config.bg_eAmod_mode && sd->bg_id && (g = bg_guild_get(sd->bg_id)) != NULL) )
		g = guild_search(sd->status.guild_id);

	if( g == NULL )
		return 0;

	fd = sd->fd;
	WFIFOHEAD(fd, 6 + MAX_GUILDSKILL*37);
	WFIFOW(fd,0) = 0x0162;
	WFIFOW(fd,4) = g->skill_point;
	for(i = 0, c = 0; i < MAX_GUILDSKILL; i++)
	{
		if(g->skill[i].id > 0 && guild_check_skill_require(g, g->skill[i].id))
		{
			int id = g->skill[i].id;
			int p = 6 + c*37;
			WFIFOW(fd,p+0) = id; 
			WFIFOW(fd,p+2) = skill_get_inf(id);
			WFIFOW(fd,p+4) = 0;
			WFIFOW(fd,p+6) = g->skill[i].lv;
			WFIFOW(fd,p+8) = skill_get_sp(id, g->skill[i].lv);
			WFIFOW(fd,p+10) = skill_get_range(id, g->skill[i].lv);
			safestrncpy((char*)WFIFOP(fd,p+12), skill_get_name(id), NAME_LENGTH);
			WFIFOB(fd,p+36)= (g->skill[i].lv < guild_skill_get_max(id) && sd == g->member[0].sd) ? 1 : 0;
			c++;
		}
	}
	WFIFOW(fd,2) = 6 + c*37;
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}

/*==========================================
 * Sends guild notice to client
 * R 016f <str1z>.60B <str2z>.120B
 *------------------------------------------*/
int clif_guild_notice(struct map_session_data* sd, struct guild* g)
{
	int fd;

	nullpo_ret(sd);
	nullpo_ret(g);

	fd = sd->fd;

	if ( !session_isActive(fd) )
		return 0;
 
	if(g->mes1[0] == '\0' && g->mes2[0] == '\0')
		return 0;

	WFIFOHEAD(fd,packet_len(0x16f));
	WFIFOW(fd,0) = 0x16f;
	memcpy(WFIFOP(fd,2), g->mes1, MAX_GUILDMES1);
	memcpy(WFIFOP(fd,62), g->mes2, MAX_GUILDMES2);
	WFIFOSET(fd,packet_len(0x16f));
	return 0;
}

/*==========================================
 * �M���h�����o���U
 *------------------------------------------*/
int clif_guild_invite(struct map_session_data *sd,struct guild *g)
{
	int fd;

	nullpo_ret(sd);
	nullpo_ret(g);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x16a));
	WFIFOW(fd,0)=0x16a;
	WFIFOL(fd,2)=g->guild_id;
	memcpy(WFIFOP(fd,6),g->name,NAME_LENGTH);
	WFIFOSET(fd,packet_len(0x16a));
	return 0;
}
/*==========================================
 * Reply to invite request
 * Flag:
 * 0 = Already in guild.
 * 1 = Offer rejected.
 * 2 = Offer accepted.
 * 3 = Guild full.
 *------------------------------------------*/
int clif_guild_inviteack(struct map_session_data *sd,int flag)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x169));
	WFIFOW(fd,0)=0x169;
	WFIFOB(fd,2)=flag;
	WFIFOSET(fd,packet_len(0x169));
	return 0;
}

/*==========================================
 * �M���h�����o�E�ޒʒm
 *------------------------------------------*/
int clif_guild_leave(struct map_session_data *sd,const char *name,const char *mes)
{
	unsigned char buf[128];

	nullpo_ret(sd);

	WBUFW(buf, 0)=0x15a;
	memcpy(WBUFP(buf, 2),name,NAME_LENGTH);
	memcpy(WBUFP(buf,26),mes,40);
	clif_send(buf,packet_len(0x15a),&sd->bl,GUILD_NOBG);
	return 0;
}

/*==========================================
 * �M���h�����o�Ǖ��ʒm
 *------------------------------------------*/
void clif_guild_expulsion(struct map_session_data* sd, const char* name, const char* mes, int account_id)
{
	unsigned char buf[128];
#if PACKETVER < 20100803
	const unsigned short cmd = 0x15c;
#else
	const unsigned short cmd = 0x839;
#endif

	nullpo_retv(sd);

	WBUFW(buf,0) = cmd;
	safestrncpy((char*)WBUFP(buf,2), name, NAME_LENGTH);
	safestrncpy((char*)WBUFP(buf,26), mes, 40);
#if PACKETVER < 20100803
	memset(WBUFP(buf,66), 0, NAME_LENGTH); // account name (not used for security reasons)
#endif
	clif_send(buf, packet_len(cmd), &sd->bl, GUILD_NOBG);
}

/*==========================================
 * �M���h�Ǖ������o���X�g
 *------------------------------------------*/
void clif_guild_expulsionlist(struct map_session_data* sd)
{
#if PACKETVER < 20100803
	const int offset = NAME_LENGTH*2+40;
#else
	const int offset = NAME_LENGTH+40;
#endif
	int fd, i, c = 0;
	struct guild* g;

	nullpo_retv(sd);

	if( !(battle_config.bg_eAmod_mode && sd->bg_id && (g = bg_guild_get(sd->bg_id)) != NULL) )
		g = guild_search(sd->status.guild_id);

	if( g == NULL ) return;

	fd = sd->fd;

	WFIFOHEAD(fd,4 + MAX_GUILDEXPULSION * offset);
	WFIFOW(fd,0) = 0x163;

	for( i = 0; i < MAX_GUILDEXPULSION; i++ )
	{
		struct guild_expulsion* e = &g->expulsion[i];

		if( e->account_id > 0 )
		{
			memcpy(WFIFOP(fd,4 + c*offset), e->name, NAME_LENGTH);
#if PACKETVER < 20100803
			memset(WFIFOP(fd,4 + c*offset+24), 0, NAME_LENGTH); // account name (not used for security reasons)
			memcpy(WFIFOP(fd,4 + c*offset+48), e->mes, 40);
#else
			memcpy(WFIFOP(fd,4 + c*offset+24), e->mes, 40);
#endif
			c++;
		}
	}
	WFIFOW(fd,2) = 4 + c*offset;
	WFIFOSET(fd,WFIFOW(fd,2));
}


/// Notification of a guild chat message (ZC_GUILD_CHAT)
/// 017f <packet len>.W <message>.?B
void clif_guild_message(struct guild *g,int account_id,const char *mes,int len)
{// TODO: account_id is not used, candidate for deletion? [Ai4rei]
	struct map_session_data *sd;
	uint8 buf[256];

	if( len == 0 )
	{
		return;
	}
	else if( len > sizeof(buf)-5 )
	{
		ShowWarning("clif_guild_message: Truncated message '%s' (len=%d, max=%d, guild_id=%d).\n", mes, len, sizeof(buf)-5, g->guild_id);
		len = sizeof(buf)-5;
	}

	WBUFW(buf, 0) = 0x17f;
	WBUFW(buf, 2) = len + 5;
	safestrncpy((char*)WBUFP(buf,4), mes, len+1);

	if ((sd = guild_getavailablesd(g)) != NULL)
		clif_send(buf, WBUFW(buf,2), &sd->bl, GUILD_NOBG);
}


/*==========================================
 * �M���h�X�L������U��ʒm
 *------------------------------------------*/
int clif_guild_skillup(struct map_session_data *sd,int skill_num,int lv)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,11);
	WFIFOW(fd,0) = 0x10e;
	WFIFOW(fd,2) = skill_num;
	WFIFOW(fd,4) = lv;
	WFIFOW(fd,6) = skill_get_sp(skill_num,lv);
	WFIFOW(fd,8) = skill_get_range(skill_num,lv);
	WFIFOB(fd,10) = 1;
	WFIFOSET(fd,11);
	return 0;
}
/*==========================================
 * �M���h�����v��
 *------------------------------------------*/
int clif_guild_reqalliance(struct map_session_data *sd,int account_id,const char *name)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x171));
	WFIFOW(fd,0)=0x171;
	WFIFOL(fd,2)=account_id;
	memcpy(WFIFOP(fd,6),name,NAME_LENGTH);
	WFIFOSET(fd,packet_len(0x171));
	return 0;
}
/*==========================================
 * Reply to alliance request.
 * Flag values are:
 * 0: Already allied.
 * 1: You rejected the offer.
 * 2: You accepted the offer.
 * 3: They have too any alliances
 * 4: You have too many alliances.
 *------------------------------------------*/
int clif_guild_allianceack(struct map_session_data *sd,int flag)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x173));
	WFIFOW(fd,0)=0x173;
	WFIFOL(fd,2)=flag;
	WFIFOSET(fd,packet_len(0x173));
	return 0;
}
/*==========================================
 * �M���h�֌W�����ʒm
 *------------------------------------------*/
int clif_guild_delalliance(struct map_session_data *sd,int guild_id,int flag)
{
	int fd;

	nullpo_ret(sd);

	fd = sd->fd;
	if (fd <= 0)
		return 0;
	WFIFOHEAD(fd,packet_len(0x184));
	WFIFOW(fd,0)=0x184;
	WFIFOL(fd,2)=guild_id;
	WFIFOL(fd,6)=flag;
	WFIFOSET(fd,packet_len(0x184));
	return 0;
}
/*==========================================
 * Reply to opposition request
 * Flag:
 * 0 = Antagonist has been set.
 * 1 = Guild has too many Antagonists.
 * 2 = Already set as an Antagonist.
 *------------------------------------------*/
int clif_guild_oppositionack(struct map_session_data *sd,int flag)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x181));
	WFIFOW(fd,0)=0x181;
	WFIFOB(fd,2)=flag;
	WFIFOSET(fd,packet_len(0x181));
	return 0;
}
/*==========================================
 * �M���h�֌W�ǉ�
 *------------------------------------------*/
/*int clif_guild_allianceadded(struct guild *g,int idx)
{
	unsigned char buf[64];
	WBUFW(fd,0)=0x185;
	WBUFL(fd,2)=g->alliance[idx].opposition;
	WBUFL(fd,6)=g->alliance[idx].guild_id;
	memcpy(WBUFP(fd,10),g->alliance[idx].name,NAME_LENGTH);
	clif_send(buf,packet_len(0x185),guild_getavailablesd(g),GUILD);
	return 0;
}*/

/*==========================================
 * �M���h���U�ʒm
 *------------------------------------------*/
int clif_guild_broken(struct map_session_data *sd,int flag)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x15e));
	WFIFOW(fd,0)=0x15e;
	WFIFOL(fd,2)=flag;
	WFIFOSET(fd,packet_len(0x15e));
	return 0;
}

/*==========================================
 * �G���[�V����
 *------------------------------------------*/
void clif_emotion(struct block_list *bl,int type)
{
	unsigned char buf[8];

	nullpo_retv(bl);

	WBUFW(buf,0)=0xc0;
	WBUFL(buf,2)=bl->id;
	WBUFB(buf,6)=type;
	clif_send(buf,packet_len(0xc0),bl,AREA);
}

/*==========================================
 * �g�[�L�[�{�b�N�X
 *------------------------------------------*/
void clif_talkiebox(struct block_list* bl, const char* talkie)
{
	unsigned char buf[86];
	nullpo_retv(bl);

	WBUFW(buf,0) = 0x191;
	WBUFL(buf,2) = bl->id;
	safestrncpy((char*)WBUFP(buf,6),talkie,MESSAGE_SIZE);
	clif_send(buf,packet_len(0x191),bl,AREA);
}

/*==========================================
 * �����G�t�F�N�g
 *------------------------------------------*/
void clif_wedding_effect(struct block_list *bl)
{
	unsigned char buf[6];

	nullpo_retv(bl);

	WBUFW(buf,0) = 0x1ea;
	WBUFL(buf,2) = bl->id;
	clif_send(buf, packet_len(0x1ea), bl, AREA);
}
/*==========================================
 * ?�Ȃ��Ɉ��������g�p�����O����
 *------------------------------------------*/

void clif_callpartner(struct map_session_data *sd)
{
	unsigned char buf[26];
	const char *p;

	nullpo_retv(sd);

	if(sd->status.partner_id){
		WBUFW(buf,0)=0x1e6;
		p = map_charid2nick(sd->status.partner_id);
		if(p){
			memcpy(WBUFP(buf,2),p,NAME_LENGTH);
		}else{
			WBUFB(buf,2) = 0;
		}
		clif_send(buf,packet_len(0x1e6),&sd->bl,AREA);
	}
}


/*==========================================
 * Marry [DracoRPG]
 *------------------------------------------
void clif_marriage_process(struct map_session_data *sd)
{
	int fd;
	nullpo_retv(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x1e4));
	WFIFOW(fd,0)=0x1e4;
	WFIFOSET(fd,packet_len(0x1e4));
}
*/


/*==========================================
 * Notice of divorce
 *------------------------------------------*/
void clif_divorced(struct map_session_data* sd, const char* name)
{
	int fd;
	nullpo_retv(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x205));
	WFIFOW(fd,0)=0x205;
	memcpy(WFIFOP(fd,2), name, NAME_LENGTH);
	WFIFOSET(fd, packet_len(0x205));
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_ReqMarriage(int fd, struct map_session_data *sd)
{
	nullpo_retv(sd);

	WFIFOHEAD(fd,packet_len(0x1e2));
	WFIFOW(fd,0)=0x1e2;
	WFIFOSET(fd, packet_len(0x1e2));
}

/*==========================================
 *
 *------------------------------------------*/
void clif_disp_onlyself(struct map_session_data *sd, const char *mes, int len)
{
	clif_disp_message(&sd->bl, mes, len, SELF);
}

/*==========================================
 * Displays a message using the guild-chat colors to the specified targets. [Skotlex]
 *------------------------------------------*/
void clif_disp_message(struct block_list* src, const char* mes, int len, enum send_target target)
{
	unsigned char buf[256];

	if( len == 0 )
	{
		return;
	}
	else if( len > sizeof(buf)-5 )
	{
		ShowWarning("clif_disp_message: Truncated message '%s' (len=%d, max=%d, aid=%d).\n", mes, len, sizeof(buf)-5, src->id);
		len = sizeof(buf)-5;
	}

	WBUFW(buf, 0) = 0x17f;
	WBUFW(buf, 2) = len + 5;
	safestrncpy((char*)WBUFP(buf,4), mes, len+1);
	clif_send(buf, WBUFW(buf,2), src, target);
}

/*==========================================
 *
 *------------------------------------------*/
int clif_GM_kickack(struct map_session_data *sd, int id)
{
	int fd;

	nullpo_ret(sd);

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0xcd));
	WFIFOW(fd,0) = 0xcd;
	WFIFOL(fd,2) = id;
	WFIFOSET(fd, packet_len(0xcd));
	return 0;
}

void clif_GM_kick(struct map_session_data *sd,struct map_session_data *tsd)
{
	int fd = tsd->fd;

	if( fd > 0 )
		clif_authfail_fd(fd, 15);
	else
		map_quit(tsd);

	if( sd )
		clif_GM_kickack(sd,tsd->status.account_id);
}

/// Displays various manner-related status messages
/// R 014a <type>.L
/// type: 0 - "A manner point has been successfully aligned."
///       1 - ?
///       2 - ?
///       3 - "Chat Block has been applied by GM due to your ill-mannerous action."
///       4 - "Automated Chat Block has been applied due to Anti-Spam System."
///       5 - "You got a good point from %s."
void clif_manner_message(struct map_session_data* sd, uint32 type)
{
	int fd;
	nullpo_retv(sd);
	
	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0x14a));
	WFIFOW(fd,0) = 0x14a;
	WFIFOL(fd,2) = type;
	WFIFOSET(fd, packet_len(0x14a));
}

/// Followup to 0x14a type 3/5, informs who did the manner adjustment action.
/// R 014b <type>.B <GM name>.24B
/// type: 0 - positive (unmute)
///       1 - negative (mute)
void clif_GM_silence(struct map_session_data* sd, struct map_session_data* tsd, uint8 type)
{
	int fd;	
	nullpo_retv(sd);
	nullpo_retv(tsd);

	fd = tsd->fd;
	WFIFOHEAD(fd,packet_len(0x14b));
	WFIFOW(fd,0) = 0x14b;
	WFIFOB(fd,2) = type;
	safestrncpy((char*)WFIFOP(fd,3), sd->status.name, NAME_LENGTH);
	WFIFOSET(fd, packet_len(0x14b));
}

/*==========================================
 * Wis���ۋ�����
 *------------------------------------------*/
int clif_wisexin(struct map_session_data *sd,int type,int flag)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0xd1));
	WFIFOW(fd,0)=0xd1;
	WFIFOB(fd,2)=type;
	WFIFOB(fd,3)=flag;
	WFIFOSET(fd,packet_len(0xd1));

	return 0;
}
/*==========================================
 * Wis�S���ۋ�����
 *------------------------------------------*/
int clif_wisall(struct map_session_data *sd,int type,int flag)
{
	int fd;

	nullpo_ret(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0xd2));
	WFIFOW(fd,0)=0xd2;
	WFIFOB(fd,2)=type;
	WFIFOB(fd,3)=flag;
	WFIFOSET(fd,packet_len(0xd2));

	return 0;
}

/*==========================================
 * Play a BGM! [Rikter/Yommy]
 *------------------------------------------*/
void clif_playBGM(struct map_session_data* sd, const char* name)
{
	int fd;

	nullpo_retv(sd);

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0x7fe));
	WFIFOW(fd,0) = 0x7fe;
	safestrncpy((char*)WFIFOP(fd,2), name, NAME_LENGTH);
	WFIFOSET(fd,packet_len(0x7fe));
}

/*==========================================
 * �T�E���h�G�t�F�N�g
 *------------------------------------------*/
void clif_soundeffect(struct map_session_data* sd, struct block_list* bl, const char* name, int type)
{
	int fd;

	nullpo_retv(sd);
	nullpo_retv(bl);

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0x1d3));
	WFIFOW(fd,0) = 0x1d3;
	safestrncpy((char*)WFIFOP(fd,2), name, NAME_LENGTH);
	WFIFOB(fd,26) = type;
	WFIFOL(fd,27) = 0;
	WFIFOL(fd,31) = bl->id;
	WFIFOSET(fd,packet_len(0x1d3));

	return;
}

int clif_soundeffectall(struct block_list* bl, const char* name, int type, enum send_target coverage)
{
	unsigned char buf[40];

	nullpo_ret(bl);

	WBUFW(buf,0) = 0x1d3;
	safestrncpy((char*)WBUFP(buf,2), name, NAME_LENGTH);
	WBUFB(buf,26) = type;
	WBUFL(buf,27) = 0;
	WBUFL(buf,31) = bl->id;
	clif_send(buf, packet_len(0x1d3), bl, coverage);

	return 0;
}

// displaying special effects (npcs, weather, etc) [Valaris]
int clif_specialeffect(struct block_list* bl, int type, enum send_target target)
{
	unsigned char buf[24];

	nullpo_ret(bl);

	memset(buf, 0, packet_len(0x1f3));

	WBUFW(buf,0) = 0x1f3;
	WBUFL(buf,2) = bl->id;
	WBUFL(buf,6) = type;

	clif_send(buf, packet_len(0x1f3), bl, target);

	if (disguised(bl)) {
		WBUFL(buf,2) = -bl->id;
		clif_send(buf, packet_len(0x1f3), bl, SELF);
	}
	return 0;
}

void clif_specialeffect_single(struct block_list* bl, int type, int fd)
{
	WFIFOHEAD(fd,10);
	WFIFOW(fd,0) = 0x1f3;
	WFIFOL(fd,2) = bl->id;
	WFIFOL(fd,6) = type;
	WFIFOSET(fd,10);
}

/******************************************************
 * W.<packet> W.<LENGTH> L.<ID> L.<COLOR> S.<TEXT>
 * Mob/NPC Color Talk [SnakeDrak]
 ******************************************************/
int clif_messagecolor(struct block_list* bl, unsigned long color, const char* msg)
{
	unsigned short msg_len = strlen(msg) + 1;
	uint8 buf[256];
	color = (color & 0x0000FF) << 16 | (color & 0x00FF00) | (color & 0xFF0000) >> 16; // RGB to BGR

	nullpo_ret(bl);

	if( msg_len > sizeof(buf)-12 )
	{
		ShowWarning("clif_messagecolor: Truncating too long message '%s' (len=%u).\n", msg, msg_len);
		msg_len = sizeof(buf)-12;
	}

	WBUFW(buf,0) = 0x2C1;
	WBUFW(buf,2) = msg_len + 12;
	WBUFL(buf,4) = bl->id;
	WBUFL(buf,8) = color;
	memcpy(WBUFP(buf,12), msg, msg_len);

	clif_send(buf, WBUFW(buf,2), bl, AREA_CHAT_WOC);

	return 0;
}

// messages (from mobs/npcs) [Valaris]
void clif_message(struct block_list* bl, const char* msg)
{
	unsigned short msg_len = strlen(msg) + 1;
	uint8 buf[256];

	nullpo_retv(bl);

	if( msg_len > sizeof(buf)-8 )
	{
		ShowWarning("clif_message: Truncating too long message '%s' (len=%u).\n", msg, msg_len);
		msg_len = sizeof(buf)-8;
	}

	WBUFW(buf,0) = 0x8d;
	WBUFW(buf,2) = msg_len + 8;
	WBUFL(buf,4) = bl->id;
	safestrncpy((char*)WBUFP(buf,8), msg, msg_len);

	clif_send(buf, WBUFW(buf,2), bl, AREA_CHAT_WOC);
}

// refresh the client's screen, getting rid of any effects
int clif_refresh(struct map_session_data *sd)
{
	nullpo_retr(-1, sd);

	clif_changemap(sd,sd->mapindex,sd->bl.x,sd->bl.y);
	clif_inventorylist(sd);
	if(pc_iscarton(sd)) {
		clif_cartlist(sd);
		clif_updatestatus(sd,SP_CARTINFO);
	}
	clif_updatestatus(sd,SP_WEIGHT);
	clif_updatestatus(sd,SP_MAXWEIGHT);
	clif_updatestatus(sd,SP_STR);
	clif_updatestatus(sd,SP_AGI);
	clif_updatestatus(sd,SP_VIT);
	clif_updatestatus(sd,SP_INT);
	clif_updatestatus(sd,SP_DEX);
	clif_updatestatus(sd,SP_LUK);
	if (sd->spiritball)
		clif_spiritball_single(sd->fd, sd);
	if (sd->vd.cloth_color)
		clif_refreshlook(&sd->bl,sd->bl.id,LOOK_CLOTHES_COLOR,sd->vd.cloth_color,SELF);
	if(merc_is_hom_active(sd->hd))
		clif_send_homdata(sd,0,0);
	if( sd->md )
	{
		clif_mercenary_info(sd);
		clif_mercenary_skillblock(sd);
	}
	if( sd->ed )
		clif_elemental_info(sd);
	map_foreachinrange(clif_getareachar,&sd->bl,AREA_SIZE,BL_ALL,sd);
	clif_weather_check(sd);
	if( sd->chatID )
		chat_leavechat(sd,0);
	if( sd->state.vending )
		clif_openvending(sd, sd->bl.id, sd->vending);
	if( pc_issit(sd) )
		clif_sitting(&sd->bl,false); // just send to self, not area
	if( pc_isdead(sd) ) //When you refresh, resend the death packet.
		clif_clearunit_single(sd->bl.id,CLR_DEAD,sd->fd);
	else
		clif_changed_dir(&sd->bl, SELF);

	// unlike vending, resuming buyingstore crashes the client.
	buyingstore_close(sd);
	clif_sendaurastoone(sd,sd);

#ifndef TXT_ONLY
	mail_clear(sd);
#endif

	return 0;
}

int clif_mobnameack_sub(struct block_list *bl, va_list ap)
{
	struct map_session_data *sd;
	unsigned char *buf, *sbuf;
	int fd, len, is_hp_update;

	nullpo_ret(bl);
	nullpo_ret(sd = (struct map_session_data *)bl);
	if( !(fd = sd->fd) || session[fd] == NULL ) return 0;

	buf = va_arg(ap,unsigned char*);
	len = va_arg(ap,int);
	sbuf = va_arg(ap,unsigned char*);
	is_hp_update = va_arg(ap,int);

	if( is_hp_update && !sd->state.view_mob_info )
		return 0; // Do not send info to normal users

	if( sd->state.view_mob_info )
	{
		len = packet_len(0x195);
		WFIFOHEAD(fd,len);
		memcpy(WFIFOP(fd,0),sbuf,len);
	}
	else
	{
		WFIFOHEAD(fd,len);
		memcpy(WFIFOP(fd,0),buf,len);
	}

	WFIFOSET(fd,len);
	return 0;
}

int clif_mobnameack(struct map_session_data *sd, struct mob_data *md, int is_hp_update)
{
	unsigned char buf[102], sbuf[102];
	struct guild *g;
	struct battleground_data *bg;
	int index, cmd, fd, len;

	if( !md ) return 0;
	index = map[md->bl.m].region;

	// Basic Packet Header
	WBUFW(buf,0) = cmd = 0x95;
	WBUFW(sbuf,0) = 0x195;
	WBUFL(buf,2) = WBUFL(sbuf,2) = md->bl.id;
	memcpy(WBUFP(buf,6), md->name, NAME_LENGTH);
	memcpy(WBUFP(sbuf,6), md->name, NAME_LENGTH);

	// Building Monster Name
	if( md->guardian_data && md->guardian_data->guild_id )
	{
		WBUFW(buf,0) = cmd = 0x195;
		WBUFB(buf,30) = WBUFB(sbuf,30) = 0;
		memcpy(WBUFP(buf,54), md->guardian_data->guild_name, NAME_LENGTH);
		memcpy(WBUFP(buf,78), md->guardian_data->castle->castle_name, NAME_LENGTH);
		memcpy(WBUFP(sbuf,54), md->guardian_data->guild_name, NAME_LENGTH);
		memcpy(WBUFP(sbuf,78), md->guardian_data->castle->castle_name, NAME_LENGTH);
	}
	else if( battle_config.bg_eAmod_mode && md->bg_id && (bg = bg_team_search(md->bg_id)) != NULL && (g = bg->g) != NULL )
	{
		WBUFW(buf,0) = cmd = 0x195;
		WBUFB(buf,30) = WBUFB(sbuf,30) = 0;
		memcpy(WBUFP(buf,54), g->name, NAME_LENGTH);
		memcpy(WBUFP(buf,78), g->position[0].name, NAME_LENGTH);
		memcpy(WBUFP(sbuf,54), g->name, NAME_LENGTH);
		memcpy(WBUFP(sbuf,78), g->position[0].name, NAME_LENGTH);
	}
	else if( index && md->option.is_war && md->option.guild_id && (g = guild_search(md->option.guild_id)) != NULL )
	{
		WBUFW(buf,0) = cmd = 0x195;
		WBUFB(buf,30) = WBUFB(sbuf,30) = 0;
		memcpy(WBUFP(buf,54), g->name, NAME_LENGTH);
		memcpy(WBUFP(buf,78), region[index].name, NAME_LENGTH);
		memcpy(WBUFP(sbuf,54), g->name, NAME_LENGTH);
		memcpy(WBUFP(sbuf,78), region[index].name, NAME_LENGTH);
	}
	else if( battle_config.show_mob_info || md->option.hp_show > 1 )
	{
		char mobhp[50], *str_p = mobhp;
		WBUFW(buf,0) = cmd = 0x195;
		if( battle_config.show_mob_info&4 )
			str_p += sprintf(str_p, "Lv. %d | ", md->level);
		if( battle_config.show_mob_info&1 || md->option.hp_show == 2 )
			str_p += sprintf(str_p, "HP: %u/%u | ", md->status.hp, md->status.max_hp);
		if( battle_config.show_mob_info&2 || md->option.hp_show == 3 )
			str_p += sprintf(str_p, "HP: %d%% | ", get_percentage(md->status.hp, md->status.max_hp));
		if( str_p != mobhp )
		{
			*(str_p-3) = '\0'; //Remove trailing space + pipe.
			memcpy(WBUFP(buf,30), mobhp, NAME_LENGTH);
			memcpy(WBUFP(sbuf,30), mobhp, NAME_LENGTH);
			WBUFB(buf,54) = WBUFB(sbuf,54) = 0;
			memcpy(WBUFP(buf,78), mobhp, NAME_LENGTH);
			memcpy(WBUFP(sbuf,78), mobhp, NAME_LENGTH);
			is_hp_update = 0; // To notify everybody around
		}
	}
	else
	{ // Build the "view_mob_hp" feature
		char mobhp[NAME_LENGTH];
		snprintf(mobhp, NAME_LENGTH, "HP: %u/%u (%d%%)", md->status.hp, md->status.max_hp, get_percentage(md->status.hp, md->status.max_hp));
		memcpy(WBUFP(sbuf,30), mobhp, NAME_LENGTH);
		WBUFB(sbuf,54) = 0;
		memcpy(WBUFP(sbuf,78), mobhp, NAME_LENGTH);
	}

	if( sd )
	{ // Single User
		fd = sd->fd;
		if( sd->state.view_mob_info )
		{ // Send Special Buff
			len = packet_len(0x195);
			WFIFOHEAD(fd,len);
			memcpy(WFIFOP(fd,0),sbuf,len);
		}
		else
		{ // Send Normal Buff
			len = packet_len(cmd);
			WFIFOHEAD(fd,len);
			memcpy(WFIFOP(fd,0),buf,len);
		}

		WFIFOSET(fd,len);
		return 1;
	}

	len = packet_len(cmd);
	map_foreachinarea(clif_mobnameack_sub, md->bl.m, md->bl.x - AREA_SIZE, md->bl.y - AREA_SIZE, md->bl.x + AREA_SIZE, md->bl.y + AREA_SIZE, BL_PC, buf, len, sbuf, is_hp_update);
	return 0;
}

/// Updates the object's (bl) name on client (ZC_ACK_REQNAME/ZC_ACK_REQNAMEALL)
/// 0095 <unit id>.L <char name>.24B
/// 0195 <unit id>.L <char name>.24B <party name>.24B <guild name>.24B <position name>.24B
int clif_charnameack (struct map_session_data *sd, struct block_list *bl)
{
	unsigned char buf[103];
	int cmd = 0x95, i, ps = -1, fd = 0;

	nullpo_ret(bl);
	if( bl->type == BL_MOB )
		return clif_mobnameack(sd, (struct mob_data *)bl, 0);

	if( sd ) fd = sd->fd;
	WBUFW(buf,0) = cmd;
	WBUFL(buf,2) = bl->id;

	switch( bl->type )
	{
	case BL_PC:
		{
			struct map_session_data *ssd = (struct map_session_data *)bl;
			struct party_data *p = NULL;
			struct guild *g = NULL;
			
			//Requesting your own "shadow" name. [Skotlex]
			if (ssd->fd == fd && ssd->disguise)
				WBUFL(buf,2) = -bl->id;

			if( ssd->fakename[0] )
				memcpy(WBUFP(buf,6), ssd->fakename, NAME_LENGTH);
			else
				memcpy(WBUFP(buf,6), ssd->status.name, NAME_LENGTH); // Cambiado para Fakename mostrando Guild Name

			if( ssd->status.party_id )
				p = party_search(ssd->status.party_id);

			if( battle_config.bg_eAmod_mode && ssd->bg_id )
			{
				g = bg_guild_get(ssd->bg_id);
				ps = ssd->bmaster_flag ? 0 : 1;
			}
			else if( ssd->status.guild_id && (g = guild_search(ssd->status.guild_id)) != NULL )
			{
				ARR_FIND(0, g->max_member, i, g->member[i].account_id == ssd->status.account_id && g->member[i].char_id == ssd->status.char_id);
				if( i < g->max_member ) ps = g->member[i].position;
			}

			if (p == NULL && g == NULL)
				break;
			
			WBUFW(buf, 0) = cmd = 0x195;
			if (p)
				memcpy(WBUFP(buf,30), p->party.name, NAME_LENGTH);
			else
				WBUFB(buf,30) = 0;
			
			if (g && ps >= 0 && ps < MAX_GUILDPOSITION)
			{
				memcpy(WBUFP(buf,54), g->name,NAME_LENGTH);
				memcpy(WBUFP(buf,78), g->position[ps].name, NAME_LENGTH);
			} else { //Assume no guild.
				WBUFB(buf,54) = 0;
				WBUFB(buf,78) = 0;
			}
		}
		break;
	//[blackhole89]
	case BL_HOM:
		memcpy(WBUFP(buf,6), ((TBL_HOM*)bl)->homunculus.name, NAME_LENGTH);
		break;
	case BL_MER:
		memcpy(WBUFP(buf,6), ((TBL_MER*)bl)->db->name, NAME_LENGTH);
		break;
	case BL_ELEM:
		memcpy(WBUFP(buf,6), ((TBL_ELEM*)bl)->db->name, NAME_LENGTH);
		break;
	case BL_PET:
		memcpy(WBUFP(buf,6), ((TBL_PET*)bl)->pet.name, NAME_LENGTH);
		break;
	case BL_NPC:
		memcpy(WBUFP(buf,6), ((TBL_NPC*)bl)->name, NAME_LENGTH);
		break;
	case BL_CHAT:	//FIXME: Clients DO request this... what should be done about it? The chat's title may not fit... [Skotlex]
//		memcpy(WBUFP(buf,6), (struct chat*)->title, NAME_LENGTH);
//		break;
		return 0;
	default:
		ShowError("clif_charnameack: bad type %d(%d)\n", bl->type, bl->id);
		return 0;
	}

	// if no receipient specified just update nearby clients
	if (fd == 0)
		clif_send(buf, packet_len(cmd), bl, AREA);
	else {
		WFIFOHEAD(fd, packet_len(cmd));
		memcpy(WFIFOP(fd, 0), buf, packet_len(cmd));
		WFIFOSET(fd, packet_len(cmd));
	}

	return 0;
}

//Used to update when a char leaves a party/guild. [Skotlex]
//Needed because when you send a 0x95 packet, the client will not remove the cached party/guild info that is not sent.
int clif_charnameupdate (struct map_session_data *ssd)
{
	unsigned char buf[103];
	int cmd = 0x195, ps = -1, i;
	struct party_data *p = NULL;
	struct guild *g = NULL;

	nullpo_ret(ssd);
	WBUFW(buf,0) = cmd;
	WBUFL(buf,2) = ssd->bl.id;

	if( strlen(ssd->fakename) > 1 )
		memcpy(WBUFP(buf,6), ssd->fakename, NAME_LENGTH);
	else
		memcpy(WBUFP(buf,6), ssd->status.name, NAME_LENGTH);

	if( ssd->status.party_id > 0 )
		p = party_search(ssd->status.party_id);

	if( battle_config.bg_eAmod_mode && ssd->bg_id > 0 )
	{
		g = bg_guild_get(ssd->bg_id);
		ps = ssd->bmaster_flag ? 0 : 1;
	}
	else if( ssd->status.guild_id > 0 && (g = guild_search(ssd->status.guild_id)) != NULL )
	{
		ARR_FIND(0, g->max_member, i, g->member[i].account_id == ssd->status.account_id && g->member[i].char_id == ssd->status.char_id);
		if( i < g->max_member ) ps = g->member[i].position;
	}

	if( p )
		memcpy(WBUFP(buf,30), p->party.name, NAME_LENGTH);
	else
		WBUFB(buf,30) = 0;

	if( g && ps >= 0 && ps < MAX_GUILDPOSITION )
	{
		memcpy(WBUFP(buf,54), g->name,NAME_LENGTH);
		memcpy(WBUFP(buf,78), g->position[ps].name, NAME_LENGTH);
	}
	else
	{
		WBUFB(buf,54) = 0;
		WBUFB(buf,78) = 0;
	}

	// Update nearby clients
	clif_send(buf, packet_len(cmd), &ssd->bl, AREA);
	return 0;
}

/// Visually moves(instant) a character to x,y. The char moves even
/// when the target cell isn't walkable. If the char is sitting it
/// stays that way.
void clif_slide(struct block_list *bl, int x, int y)
{
	unsigned char buf[10];
	nullpo_retv(bl);

	WBUFW(buf, 0) = 0x01ff;
	WBUFL(buf, 2) = bl->id;
	WBUFW(buf, 6) = x;
	WBUFW(buf, 8) = y;
	clif_send(buf, packet_len(0x1ff), bl, AREA);

	if( disguised(bl) )
	{
		WBUFL(buf,2) = -bl->id;
		clif_send(buf, packet_len(0x1ff), bl, SELF);
	}
}

/*------------------------------------------
 * @me command by lordalfa, rewritten implementation by Skotlex
 *------------------------------------------*/
void clif_disp_overhead(struct map_session_data *sd, const char* mes)
{
	unsigned char buf[256]; //This should be more than sufficient, the theorical max is CHAT_SIZE + 8 (pads and extra inserted crap)
	int len_mes = strlen(mes)+1; //Account for \0

	if (len_mes > sizeof(buf)-8) {
		ShowError("clif_disp_overhead: Message too long (length %d)\n", len_mes);
		len_mes = sizeof(buf)-8; //Trunk it to avoid problems.
	}
	// send message to others
	WBUFW(buf,0) = 0x8d;
	WBUFW(buf,2) = len_mes + 8; // len of message + 8 (command+len+id)
	WBUFL(buf,4) = sd->bl.id;
	safestrncpy((char*)WBUFP(buf,8), mes, len_mes);
	clif_send(buf, WBUFW(buf,2), &sd->bl, AREA_CHAT_WOC);

	// send back message to the speaker
	WBUFW(buf,0) = 0x8e;
	WBUFW(buf, 2) = len_mes + 4;
	safestrncpy((char*)WBUFP(buf,4), mes, len_mes);  
	clif_send(buf, WBUFW(buf,2), &sd->bl, SELF);
}

/*==========================
 * Minimap fix [Kevin]
 * Remove dot from minimap 
 *--------------------------*/
int clif_party_xy_remove(struct map_session_data *sd)
{
	unsigned char buf[16];
	nullpo_ret(sd);
	WBUFW(buf,0)=0x107;
	WBUFL(buf,2)=sd->status.account_id;
	WBUFW(buf,6)=-1;
	WBUFW(buf,8)=-1;
	clif_send(buf,packet_len(0x107),&sd->bl,PARTY_SAMEMAP_WOS);
	return 0;
}

//Displays gospel-buff information (thanks to Rayce):
//Type determines message based on this table:
/*
	0x15 End all negative status
	0x16 Immunity to all status
	0x17 MaxHP +100%
	0x18 MaxSP +100%
	0x19 All stats +20
	0x1c Enchant weapon with Holy element
	0x1d Enchant armor with Holy element
	0x1e DEF +25%
	0x1f ATK +100%
	0x20 HIT/Flee +50
	0x28 Full strip failed because of coating (unrelated to gospel, maybe for ST_FULLSTRIP)
*/
void clif_gospel_info(struct map_session_data *sd, int type)
{
	int fd=sd->fd;
	WFIFOHEAD(fd,packet_len(0x215));
	WFIFOW(fd,0)=0x215;
	WFIFOL(fd,2)=type;
	WFIFOSET(fd, packet_len(0x215));

}

/// Multi purpose mission information packet (ZC_STARSKILL).
/// 0x20e <mapname>.24B <monster_id>.L <star>.B <result>.B
/// result:
///      0 = Star Gladiator %s has designed <mapname>'s as the %s.
///      star:
///          0 = Place of the Sun
///          1 = Place of the Moon
///          2 = Place of the Stars
///      1 = Star Gladiator %s's %s: <mapname>
///      star:
///          0 = Place of the Sun
///          1 = Place of the Moon
///          2 = Place of the Stars
///      10 = Star Gladiator %s has designed <mapname>'s as the %s.
///      star:
///          0 = Target of the Sun
///          1 = Target of the Moon
///          2 = Target of the Stars
///      11 = Star Gladiator %s's %s: <mapname used as monster name>
///      star:
///          0 = Monster of the Sun
///          1 = Monster of the Moon
///          2 = Monster of the Stars
///      20 = [TaeKwon Mission] Target Monster : <mapname used as monster name> (<star>%)
///      21 = [Taming Mission] Target Monster : <mapname used as monster name>
///      22 = [Collector Rank] Target Item : <monster_id used as item id>
///      30 = [Sun, Moon and Stars Angel] Designed places and monsters have been reset.
///      40 = Target HP : <monster_id used as HP>
void clif_starskill(struct map_session_data* sd, const char* mapname, int monster_id, unsigned char star, unsigned char result)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x20e));
	WFIFOW(fd,0) = 0x20e;
	safestrncpy((char*)WFIFOP(fd,2), mapname, NAME_LENGTH);
	WFIFOL(fd,26) = monster_id;
	WFIFOB(fd,30) = star;
	WFIFOB(fd,31) = result;
	WFIFOSET(fd,packet_len(0x20e));
}

/*==========================================
 * Info about Star Glaldiator save map [Komurka]
 * type: 1: Information, 0: Map registered
 *------------------------------------------*/
void clif_feel_info(struct map_session_data* sd, unsigned char feel_level, unsigned char type)
{
	char mapname[MAP_NAME_LENGTH_EXT];

	mapindex_getmapname_ext(mapindex_id2name(sd->feel_map[feel_level].index), mapname);
	clif_starskill(sd, mapname, 0, feel_level, type ? 1 : 0);
}

/*==========================================
 * Info about Star Glaldiator hate mob [Komurka]
 * type: 1: Register mob, 0: Information.
 *------------------------------------------*/
void clif_hate_info(struct map_session_data *sd, unsigned char hate_level,int class_, unsigned char type)
{
	if( pcdb_checkid(class_) )
	{
		clif_starskill(sd, job_name(class_), class_, hate_level, type ? 10 : 11);
	}
	else if( mobdb_checkid(class_) )
	{
		clif_starskill(sd, mob_db(class_)->jname, class_, hate_level, type ? 10 : 11);
	}
	else
	{
		ShowWarning("clif_hate_info: Received invalid class %d for this packet (char_id=%d, hate_level=%u, type=%u).\n", class_, sd->status.char_id, (unsigned int)hate_level, (unsigned int)type);
	}
}

/*==========================================
 * Info about TaeKwon Do TK_MISSION mob [Skotlex]
 *------------------------------------------*/
void clif_mission_info(struct map_session_data *sd, int mob_id, unsigned char progress)
{
	clif_starskill(sd, mob_db(mob_id)->jname, mob_id, progress, 20);
}

/*==========================================
 * Feel/Hate reset (thanks to Rayce) [Skotlex]
 *------------------------------------------*/
void clif_feel_hate_reset(struct map_session_data *sd)
{
	clif_starskill(sd, "", 0, 0, 30);
}

/*==========================================
 * Equip window (un)tick ack
 * R 02d9 <zero>.L <flag>.L
 *------------------------------------------*/
void clif_equiptickack(struct map_session_data* sd, int flag)
{
	int fd;
	nullpo_retv(sd);
	fd = sd->fd;

	WFIFOHEAD(fd, packet_len(0x2d9));
	WFIFOW(fd, 0) = 0x2d9;
	WFIFOL(fd, 2) = 0;
	WFIFOL(fd, 6) = flag;
	WFIFOSET(fd, packet_len(0x2d9));
}

/*==========================================
 * The player's 'view equip' state, sent during login
 * R 02da <flag>.B
 *------------------------------------------*/
void clif_equipcheckbox(struct map_session_data* sd)
{
	int fd;
	nullpo_retv(sd);
	fd = sd->fd;

	WFIFOHEAD(fd, packet_len(0x2da));
	WFIFOW(fd, 0) = 0x2da;
	WFIFOB(fd, 2) = (sd->status.show_equip ? 1 : 0);
	WFIFOSET(fd, packet_len(0x2da));
}

/// Sends info about a player's equipped items (ZC_EQUIPWIN_MICROSCOPE)
/// 02d7 <packet len>.W <name>.24B <class>.W <hairstyle>.W <up-viewid>.W <mid-viewid>.W <low-viewid>.W <haircolor>.W <cloth-dye>.W <gender>.B {equip item}.26B*
/// 02d7 <packet len>.W <name>.24B <class>.W <hairstyle>.W <bottom-viewid>.W <mid-viewid>.W <up-viewid>.W <haircolor>.W <cloth-dye>.W <gender>.B {equip item}.28B* (PACKETVER >= 20100629)
/// 0859 <packet len>.W <name>.24B <class>.W <hairstyle>.W <bottom-viewid>.W <mid-viewid>.W <up-viewid>.W <haircolor>.W <cloth-dye>.W <gender>.B {equip item}.28B* (PACKETVER >= 20101124)
/// 0859 <packet len>.W <name>.24B <class>.W <hairstyle>.W <bottom-viewid>.W <mid-viewid>.W <up-viewid>.W <robe>.W <haircolor>.W <cloth-dye>.W <gender>.B {equip item}.28B* (PACKETVER >= 20110111)
void clif_viewequip_ack(struct map_session_data* sd, struct map_session_data* tsd)
{
	uint8* buf;
	int i, n, fd, offset = 0;
#if PACKETVER < 20100629
	const int s = 26;
#else
	const int s = 28;
#endif
	nullpo_retv(sd);
	nullpo_retv(tsd);
	fd = sd->fd;
	
	WFIFOHEAD(fd, MAX_INVENTORY * s + 43);
	buf = WFIFOP(fd,0);

#if PACKETVER < 20101124
	WBUFW(buf, 0) = 0x2d7;
#else
	WBUFW(buf, 0) = 0x859;
#endif
	safestrncpy((char*)WBUFP(buf, 4), tsd->status.name, NAME_LENGTH);
	WBUFW(buf,28) = tsd->status.class_;
	WBUFW(buf,30) = tsd->vd.hair_style;	
	WBUFW(buf,32) = tsd->vd.head_bottom;
	WBUFW(buf,34) = tsd->vd.head_mid;
	WBUFW(buf,36) = tsd->vd.head_top;
#if PACKETVER >= 20110111
	WBUFW(buf,38) = tsd->vd.robe;
	offset+= 2;
	buf = WBUFP(buf,2);
#endif
	WBUFW(buf,38) = tsd->vd.hair_color;
	WBUFW(buf,40) = tsd->vd.cloth_color;
	WBUFB(buf,42) = tsd->vd.sex;
	
	for(i=0,n=0; i < MAX_INVENTORY; i++)
	{
		if (tsd->status.inventory[i].nameid <= 0 || tsd->inventory_data[i] == NULL)	// Item doesn't exist
			continue;
		if (!itemdb_isequip2(tsd->inventory_data[i])) // Is not equippable
			continue;
	
		// Inventory position
		WBUFW(buf, n*s+43) = i + 2;
		// Add refine, identify flag, element, etc.
		clif_item_sub(WBUFP(buf,0), n*s+45, &tsd->status.inventory[i], tsd->inventory_data[i], pc_equippoint(tsd, i));
		// Add cards
		clif_addcards(WBUFP(buf, n*s+55), &tsd->status.inventory[i]);	
		// Expiration date stuff, if all of those are set to 0 then the client doesn't show anything related (6 bytes)
		WBUFL(buf, n*s+63) = tsd->status.inventory[i].expire_time;
		WBUFW(buf, n*s+67) = tsd->status.inventory[i].bound ? 2 : 0;
#if PACKETVER >= 20100629
		if (tsd->inventory_data[i]->equip&EQP_VISIBLE)
			WBUFW(buf, n*s+69) = tsd->inventory_data[i]->look;
		else
			WBUFW(buf, n*s+69) = 0;
#endif
		n++;
	}

	WFIFOW(fd, 2) = 43+offset+n*s;	// Set length
	WFIFOSET(fd, WFIFOW(fd, 2));
}

/// Display msgstringtable.txt string (ZC_MSG)
/// R 0291 <message>.W
void clif_msg(struct map_session_data* sd, unsigned short id)
{
	int fd;
	nullpo_retv(sd);
	fd = sd->fd;

	WFIFOHEAD(fd, packet_len(0x291));
	WFIFOW(fd, 0) = 0x291;
	WFIFOW(fd, 2) = id;  // zero-based msgstringtable.txt index
	WFIFOSET(fd, packet_len(0x291));
}

void clif_msgtable_num(int fd, int line, int num)
{
#if PACKETVER >= 20090805
	int a,b;
	b = line / 255;
	a = line - ( b * 255 ) - b;

	WFIFOHEAD(fd, packet_len(0x7e2));
	WFIFOW(fd, 0) = 0x7e2;
	WFIFOB(fd, 2) = a;
	WFIFOB(fd, 3) = b;
	WFIFOL(fd, 4) = num;
	WFIFOSET(fd, packet_len(0x7e2));
#endif
}

/// View player equip request denied
void clif_viewequip_fail(struct map_session_data* sd)
{
	clif_msg(sd, 0x54d);
}

/// Validates one global/guild/party/whisper message packet and tries to recognize its components.
/// Returns true if the packet was parsed successfully.
/// Formats: 0 - <packet id>.w <packet len>.w (<name> : <message>).?B 00
///          1 - <packet id>.w <packet len>.w <name>.24B <message>.?B 00
static bool clif_process_message(struct map_session_data* sd, int format, char** name_, int* namelen_, char** message_, int* messagelen_)
{
	char *text, *name, *message;
	unsigned int packetlen, textlen, namelen, messagelen;
	int fd = sd->fd;

	*name_ = NULL;
	*namelen_ = 0;
	*message_ = NULL;
	*messagelen_ = 0;

	packetlen = RFIFOW(fd,2);
	// basic structure checks
	if( packetlen > RFIFOREST(fd) )
	{	// there has to be enough data to read
		ShowWarning("clif_process_message: Received malformed packet from player '%s' (packet length is incorrect)!\n", sd->status.name);
		return false;
	}
	if( packetlen < 4 + 1 )
	{	// 4-byte header and at least an empty string is expected
		ShowWarning("clif_process_message: Received malformed packet from player '%s' (no message data)!\n", sd->status.name);
		return false;
	}

	text = (char*)RFIFOP(fd,4);
	textlen = packetlen - 4;

	// process <name> part of the packet
	if( format == 0 )
	{// name and message are separated by ' : '
		// validate name
		name = text;
		namelen = strnlen(sd->status.name, NAME_LENGTH-1); // name length (w/o zero byte)

		if( strncmp(name, sd->status.name, namelen) || // the text must start with the speaker's name
			name[namelen] != ' ' || name[namelen+1] != ':' || name[namelen+2] != ' ' ) // followed by ' : '
		{
			//Hacked message, or infamous "client desynch" issue where they pick one char while loading another.
			ShowWarning("clif_process_message: Player '%s' sent a message using an incorrect name! Forcing a relog...\n", sd->status.name);
			set_eof(fd); // Just kick them out to correct it.
			return false;
		}

		message = name + namelen + 3;
		messagelen = textlen - namelen - 3; // this should be the message length (w/ zero byte included)
	}
	else
	{// name has fixed width
		if( textlen < NAME_LENGTH + 1 )
		{
			ShowWarning("clif_process_message: Received malformed packet from player '%s' (packet length is incorrect)!\n", sd->status.name);
			return false;
		}

		// validate name
		name = text;
		namelen = strnlen(name, NAME_LENGTH-1); // name length (w/o zero byte)

		if( name[namelen] != '\0' )
		{	// only restriction is that the name must be zero-terminated
			ShowWarning("clif_process_message: Player '%s' sent an unterminated name!\n", sd->status.name);
			return false;
		}

		message = name + NAME_LENGTH;
		messagelen = textlen - NAME_LENGTH; // this should be the message length (w/ zero byte included)
	}

	if( messagelen != strnlen(message, messagelen)+1 )
	{	// the declared length must match real length
		ShowWarning("clif_process_message: Received malformed packet from player '%s' (length is incorrect)!\n", sd->status.name);
		return false;		
	}
	// verify <message> part of the packet
	if( message[messagelen-1] != '\0' )
	{	// message must be zero-terminated
		ShowWarning("clif_process_message: Player '%s' sent an unterminated message string!\n", sd->status.name);
		return false;		
	}
	if( messagelen > CHAT_SIZE_MAX-1 )
	{	// messages mustn't be too long
		// Normally you can only enter CHATBOX_SIZE-1 letters into the chat box, but Frost Joke / Dazzler's text can be longer.
		// Also, the physical size of strings that use multibyte encoding can go multiple times over the chatbox capacity.
		// Neither the official client nor server place any restriction on the length of the data in the packet,
		// but we'll only allow reasonably long strings here. This also makes sure that they fit into the `chatlog` table.
		ShowWarning("clif_process_message: Player '%s' sent a message too long ('%.*s')!\n", sd->status.name, CHAT_SIZE_MAX-1, message);
		return false;
	}

	*name_ = name;
	*namelen_ = namelen;
	*message_ = message;
	*messagelen_ = messagelen;
	return true;
}

// ---------------------
// clif_guess_PacketVer
// ---------------------
// Parses a WantToConnection packet to try to identify which is the packet version used. [Skotlex]
// error codes:
// 0 - Success
// 1 - Unknown packet_ver
// 2 - Invalid account_id
// 3 - Invalid char_id
// 4 - Invalid login_id1 (reserved)
// 5 - Invalid client_tick (reserved)
// 6 - Invalid sex
// Only the first 'invalid' error that appears is used.
static int clif_guess_PacketVer(int fd, int get_previous, int *error)
{
	static int err = 1;
	static int packet_ver = -1;
	int cmd, packet_len, value; //Value is used to temporarily store account/char_id/sex

	if (get_previous)
	{//For quick reruns, since the normal code flow is to fetch this once to identify the packet version, then again in the wanttoconnect function. [Skotlex]
		if( error )
			*error = err;
		return packet_ver;
	}

	//By default, start searching on the default one.
	err = 1;
	packet_ver = clif_config.packet_db_ver;
	cmd = RFIFOW(fd,0);
	packet_len = RFIFOREST(fd);

#define SET_ERROR(n) \
	if( err == 1 )\
		err = n;\
//define SET_ERROR

	// FIXME: If the packet is not received at once, this will FAIL.
	// Figure out, when it happens, that only part of the packet is
	// received, or fix the function to be able to deal with that
	// case.
#define CHECK_PACKET_VER() \
	if( cmd != clif_config.connect_cmd[packet_ver] || packet_len != packet_db[packet_ver][cmd].len )\
		;/* not wanttoconnection or wrong length */\
	else if( (value=(int)RFIFOL(fd, packet_db[packet_ver][cmd].pos[0])) < START_ACCOUNT_NUM || value > END_ACCOUNT_NUM )\
	{ SET_ERROR(2); }/* invalid account_id */\
	else if( (value=(int)RFIFOL(fd, packet_db[packet_ver][cmd].pos[1])) <= 0 )\
	{ SET_ERROR(3); }/* invalid char_id */\
	/*                   RFIFOL(fd, packet_db[packet_ver][cmd].pos[2]) - don't care about login_id1 */\
	/*                   RFIFOL(fd, packet_db[packet_ver][cmd].pos[3]) - don't care about client_tick */\
	else if( (value=(int)RFIFOB(fd, packet_db[packet_ver][cmd].pos[4])) != 0 && value != 1 )\
	{ SET_ERROR(6); }/* invalid sex */\
	else\
	{\
		err = 0;\
		if( error )\
			*error = 0;\
		return packet_ver;\
	}\
//define CHECK_PACKET_VER

	CHECK_PACKET_VER();//Default packet version found.
	
	for (packet_ver = MAX_PACKET_VER; packet_ver > 0; packet_ver--)
	{	//Start guessing the version, giving priority to the newer ones. [Skotlex]
		CHECK_PACKET_VER();
	}
	if( error )
		*error = err;
	packet_ver = -1;
	return -1;
#undef SET_ERROR
#undef CHECK_PACKET_VER
}

// ------------
// clif_parse_*
// ------------
// �p�P�b�g�ǂݎ���ĐF�X����
/*==========================================
 *
 *------------------------------------------*/
void clif_parse_WantToConnection(int fd, TBL_PC* sd)
{
	struct block_list* bl;
	struct auth_node* node;
	int cmd, account_id, char_id, login_id1, sex;
	unsigned int client_tick; //The client tick is a tick, therefore it needs be unsigned. [Skotlex]
	int packet_ver;	// 5: old, 6: 7july04, 7: 13july04, 8: 26july04, 9: 9aug04/16aug04/17aug04, 10: 6sept04, 11: 21sept04, 12: 18oct04, 13: 25oct04 (by [Yor])

	if (sd) {
		ShowError("clif_parse_WantToConnection : invalid request (character already logged in)\n");
		return;
	}

	// Only valid packet version get here
	packet_ver = clif_guess_PacketVer(fd, 1, NULL);

	cmd = RFIFOW(fd,0);
	account_id  = RFIFOL(fd, packet_db[packet_ver][cmd].pos[0]);
	char_id     = RFIFOL(fd, packet_db[packet_ver][cmd].pos[1]);
	login_id1   = RFIFOL(fd, packet_db[packet_ver][cmd].pos[2]);
	client_tick = RFIFOL(fd, packet_db[packet_ver][cmd].pos[3]);
	sex         = RFIFOB(fd, packet_db[packet_ver][cmd].pos[4]);

	if( packet_ver < 5 || // reject really old client versions
			(packet_ver <= 9 && (battle_config.packet_ver_flag & 1) == 0) || // older than 6sept04
			(packet_ver > 9 && (battle_config.packet_ver_flag & 1<<(packet_ver-9)) == 0)) // version not allowed
	{// packet version rejected
		ShowInfo("Rejected connection attempt, forbidden packet version (AID/CID: '"CL_WHITE"%d/%d"CL_RESET"', Packet Ver: '"CL_WHITE"%d"CL_RESET"', IP: '"CL_WHITE"%s"CL_RESET"').\n", account_id, char_id, packet_ver, ip2str(session[fd]->client_addr, NULL));
		WFIFOHEAD(fd,packet_len(0x6a));
		WFIFOW(fd,0) = 0x6a;
		WFIFOB(fd,2) = 5; // Your Game's EXE file is not the latest version
		WFIFOSET(fd,packet_len(0x6a));
		set_eof(fd);
		return;
	}

	if( runflag != MAPSERVER_ST_RUNNING )
	{// not allowed
		clif_authfail_fd(fd,1);// server closed
		return;
	}

	//Check for double login.
	bl = map_id2bl(account_id);
	if(bl && bl->type != BL_PC) {
		ShowError("clif_parse_WantToConnection: a non-player object already has id %d, please increase the starting account number\n", account_id);
		WFIFOHEAD(fd,packet_len(0x6a));
		WFIFOW(fd,0) = 0x6a;
		WFIFOB(fd,2) = 3; // Rejected by server
		WFIFOSET(fd,packet_len(0x6a));
		set_eof(fd);
		return;
	}

	if (bl || 
		((node=chrif_search(account_id)) && //An already existing node is valid only if it is for this login.
			!(node->account_id == account_id && node->char_id == char_id && node->state == ST_LOGIN)))
	{
		clif_authfail_fd(fd, 8); //Still recognizes last connection
		return;
	}

	CREATE(sd, TBL_PC, 1);
	sd->fd = fd;
	sd->packet_ver = packet_ver;
	session[fd]->session_data = sd;

	pc_setnewpc(sd, account_id, char_id, login_id1, client_tick, sex, fd);

#if PACKETVER < 20070521
	WFIFOHEAD(fd,4);
	WFIFOL(fd,0) = sd->bl.id;
	WFIFOSET(fd,4);
#else
	WFIFOHEAD(fd,packet_len(0x283));
	WFIFOW(fd,0) = 0x283;
	WFIFOL(fd,2) = sd->bl.id;
	WFIFOSET(fd,packet_len(0x283));
#endif

	chrif_authreq(sd);
	return;
}

/*==========================================
 * 007d �N���C�A���g���}�b�v�ǂݍ��݊���
 * map�N�����ɕK�v�ȃf�[�^��S�đ������
 *------------------------------------------*/
void clif_parse_LoadEndAck(int fd,struct map_session_data *sd)
{
	short index = map[sd->bl.m].region;

	if(sd->bl.prev != NULL)
		return;
	
	if (!sd->state.active)
	{	//Character loading is not complete yet!
		//Let pc_reg_received reinvoke this when ready.
		sd->state.connect_new = 0;
		return;
	}

	if (sd->state.rewarp)
  	{	//Rewarp player.
		sd->state.rewarp = 0;
		clif_changemap(sd, sd->mapindex, sd->bl.x, sd->bl.y);
		return;
	}

	// look
#if PACKETVER < 4
	clif_changelook(&sd->bl,LOOK_WEAPON,sd->status.weapon);
	clif_changelook(&sd->bl,LOOK_SHIELD,sd->status.shield);
#else
	clif_changelook(&sd->bl,LOOK_WEAPON,0);
#endif

	if(sd->vd.cloth_color)
		clif_refreshlook(&sd->bl,sd->bl.id,LOOK_CLOTHES_COLOR,sd->vd.cloth_color,SELF);

	// item
	clif_inventorylist(sd);  // inventory list first, otherwise deleted items in pc_checkitem show up as 'unknown item'
	pc_checkitem(sd);
	
	// cart
	if(pc_iscarton(sd)) {
		clif_cartlist(sd);
		clif_updatestatus(sd,SP_CARTINFO);
	}

	// weight
	clif_updatestatus(sd,SP_WEIGHT);
	clif_updatestatus(sd,SP_MAXWEIGHT);

	// guild
	// (needs to go before clif_spawn() to show guild emblems correctly)
	if(sd->status.guild_id)
		guild_send_memberinfoshort(sd,1);

	// Set User Custom Aura if any
	if( sd->state.pvpmode && map_flag_nopvpmode(sd->bl.m) )
		pc_pvpmodeoff(sd,1,0);

	sd->view_aura = pc_get_aura(sd);

	if(battle_config.pc_invincible_time > 0) {
		if(map_flag_gvg(sd->bl.m))
			pc_setinvincibletimer(sd,battle_config.pc_invincible_time<<1);
		else
			pc_setinvincibletimer(sd,battle_config.pc_invincible_time);
	}

	if( map[sd->bl.m].users++ == 0 && battle_config.dynamic_mobs )
		map_spawnmobs(sd->bl.m);
	if( map[sd->bl.m].instance_id )
	{
		instance[map[sd->bl.m].instance_id].users++;
		instance_check_idle(map[sd->bl.m].instance_id);
	}

	if( index > 0 ) region[index].users++;

	map_addblock(&sd->bl);
	clif_spawn(&sd->bl);

	// Party
	// (needs to go after clif_spawn() to show hp bars correctly)
	if(sd->status.party_id) {
		party_send_movemap(sd);
		clif_party_hp(sd); // Show hp after displacement [LuzZza]
	}

	if( sd->bg_id ) clif_bg_hp(sd); // BattleGround System
	if( battle_config.bg_eAmod_mode && sd->state.changemap && map[sd->bl.m].flag.battleground )
	{
		clif_map_type(sd, MAPTYPE_BATTLEFIELD); // Battleground Mode
		if( map[sd->bl.m].flag.battleground >= 2 )
			clif_bg_updatescore_single(sd); // Score board only need update on change map
	}

	if( map[sd->bl.m].flag.pvp_event )
		clif_map_property(sd, MAPPROPERTY_FREEPVPZONE);
	else if( map[sd->bl.m].flag.pvp )
	{
		if(!battle_config.pk_mode) { // remove pvp stuff for pk_mode [Valaris]
			if (!map[sd->bl.m].flag.pvp_nocalcrank)
				sd->pvp_timer = add_timer(gettick()+200, pc_calc_pvprank_timer, sd->bl.id, 0);
			sd->pvp_rank = 0;
			sd->pvp_lastusers = 0;
			sd->pvp_point = 5;
			sd->pvp_won = 0;
			sd->pvp_lost = 0;
		}
		clif_map_property(sd, MAPPROPERTY_FREEPVPZONE);
	}
	else if( sd->duel_group || sd->state.pvpmode )
		clif_map_property(sd, MAPPROPERTY_FREEPVPZONE);

	if (map[sd->bl.m].flag.gvg_dungeon)
		clif_map_property(sd, MAPPROPERTY_FREEPVPZONE); //TODO: Figure out the real packet to send here.

	if( map_flag_gvg(sd->bl.m) || (battle_config.bg_eAmod_mode && map[sd->bl.m].flag.battleground) )
		clif_map_property(sd, MAPPROPERTY_AGITZONE);
	else if( battle_config.guild_wars && sd->status.guild_id )
	{ // At War!!
		struct guild *g = guild_search( sd->status.guild_id );
		if( g && g->war && !map_flag_noguildwar(sd->bl.m) )
			clif_map_property(sd, MAPPROPERTY_FREEPVPZONE);
	}


	// info about nearby objects
	// must use foreachinarea (CIRCULAR_AREA interferes with foreachinrange)
	map_foreachinarea(clif_getareachar, sd->bl.m, sd->bl.x-AREA_SIZE, sd->bl.y-AREA_SIZE, sd->bl.x+AREA_SIZE, sd->bl.y+AREA_SIZE, BL_ALL, sd);

	// pet
	if( sd->pd )
	{
		if( battle_config.pet_no_gvg && map_flag_gvg(sd->bl.m) )
		{	//Return the pet to egg. [Skotlex]
			clif_displaymessage(sd->fd, "Pets are not allowed in Guild Wars.");
			pet_menu(sd, 3); //Option 3 is return to egg.
		}
		else
		{
			map_addblock(&sd->pd->bl);
			clif_spawn(&sd->pd->bl);
			clif_send_petdata(sd,sd->pd,0,0);
			clif_send_petstatus(sd);
//			skill_unit_move(&sd->pd->bl,gettick(),1);
		}
	}

	//homunculus [blackhole89]
	if( merc_is_hom_active(sd->hd) )
	{
		if( map[sd->bl.m].flag.ancient )
			merc_hom_vaporize(sd, 0);
		else
		{
			map_addblock(&sd->hd->bl);
			clif_spawn(&sd->hd->bl);
			clif_send_homdata(sd,0,0);
			clif_hominfo(sd,sd->hd,1);
			clif_hominfo(sd,sd->hd,0); //for some reason, at least older clients want this sent twice
			clif_homskillinfoblock(sd);
			if( battle_config.hom_setting&0x8 )
				status_calc_bl(&sd->hd->bl, SCB_SPEED); //Homunc mimic their master's speed on each map change
			if( !(battle_config.hom_setting&0x2) )
				skill_unit_move(&sd->hd->bl,gettick(),1); // apply land skills immediately
		}
	}

	if( sd->md )
	{
		map_addblock(&sd->md->bl);
		clif_spawn(&sd->md->bl);
		clif_mercenary_info(sd);
		clif_mercenary_skillblock(sd);
	}

	if( sd->ed )
	{
		map_addblock(&sd->ed->bl);
		clif_spawn(&sd->ed->bl);
		clif_elemental_info(sd);
		clif_elemental_updatestatus(sd,SP_HP);
		clif_hpmeter_single(sd->fd,sd->ed->bl.id,sd->ed->battle_status.hp,sd->ed->battle_status.matk_max);
		clif_elemental_updatestatus(sd,SP_SP);
	}

	if(sd->state.connect_new) {
		int lv;
		sd->state.connect_new = 0;
		clif_skillinfoblock(sd);
		clif_hotkeys_send(sd);
		clif_updatestatus(sd,SP_BASEEXP);
		clif_updatestatus(sd,SP_NEXTBASEEXP);
		clif_updatestatus(sd,SP_JOBEXP);
		clif_updatestatus(sd,SP_NEXTJOBEXP);
		clif_updatestatus(sd,SP_SKILLPOINT);
		clif_initialstatus(sd);

		if (sd->sc.option&OPTION_FALCON)
			clif_status_load(&sd->bl, SI_FALCON, 1);

		if (sd->sc.option&OPTION_RIDING || sd->sc.option&(OPTION_RIDING_DRAGON))
			clif_status_load(&sd->bl, SI_RIDING, 1);

		if (sd->sc.option&OPTION_RIDING_WUG)
			clif_status_load(&sd->bl, SI_WUGMOUNT, 1);

		if(sd->status.manner < 0)
			sc_start(&sd->bl,SC_NOCHAT,100,0,0);

		//Auron reported that This skill only triggers when you logon on the map o.O [Skotlex]
		if ((lv = pc_checkskill(sd,SG_KNOWLEDGE)) > 0) {
			if(sd->bl.m == sd->feel_map[0].m
				|| sd->bl.m == sd->feel_map[1].m
				|| sd->bl.m == sd->feel_map[2].m)
				sc_start(&sd->bl, SC_KNOWLEDGE, 100, lv, skill_get_time(SG_KNOWLEDGE, lv));
		}

		if(sd->pd && sd->pd->pet.intimate > 900)
			clif_pet_emotion(sd->pd,(sd->pd->pet.class_ - 100)*100 + 50 + pet_hungry_val(sd->pd));

		if(merc_is_hom_active(sd->hd))
			merc_hom_init_timers(sd->hd);

		if (night_flag && map[sd->bl.m].flag.nightenabled)
		{
			sd->state.night = 1;
			clif_status_load(&sd->bl, SI_NIGHT, 1);
		}

		// Notify everyone that this char logged in [Skotlex].
		map_foreachpc(clif_friendslist_toggle_sub, sd->status.account_id, sd->status.char_id, 1);

		//Login Event
		npc_script_event(sd, NPCE_LOGIN);
	} else {
		//For some reason the client "loses" these on warp/map-change.
		clif_updatestatus(sd,SP_STR);
		clif_updatestatus(sd,SP_AGI);
		clif_updatestatus(sd,SP_VIT);
		clif_updatestatus(sd,SP_INT);
		clif_updatestatus(sd,SP_DEX);
		clif_updatestatus(sd,SP_LUK);
	
		// abort currently running script
		sd->state.using_fake_npc = 0;
		sd->state.menu_or_input = 0;
		sd->npc_menu = 0;

		if(sd->npc_id)
			npc_event_dequeue(sd);
	}

	if( sd->state.changemap )
	{// restore information that gets lost on map-change
#if PACKETVER >= 20070918
		clif_partyinvitationstate(sd);
		clif_equipcheckbox(sd);
#endif
		if( (battle_config.bg_flee_penalty != 100 || battle_config.gvg_flee_penalty != 100) &&
			(map_flag_gvg(sd->state.pmap) || map_flag_gvg(sd->bl.m) || map[sd->state.pmap].flag.battleground || map[sd->bl.m].flag.battleground) )
			status_calc_bl(&sd->bl, SCB_FLEE); //Refresh flee penalty

		if( night_flag && map[sd->bl.m].flag.nightenabled )
		{	//Display night.
			if( !sd->state.night )
			{
				sd->state.night = 1;
				clif_status_load(&sd->bl, SI_NIGHT, 1);
			}
		}
		else if( sd->state.night )
		{ //Clear night display.
			sd->state.night = 0;
			clif_status_load(&sd->bl, SI_NIGHT, 0);
		}

		if( !battle_config.bg_eAmod_mode && map[sd->bl.m].flag.battleground )
		{
			clif_map_type(sd, MAPTYPE_BATTLEFIELD); // Battleground Mode
			if( map[sd->bl.m].flag.battleground >= 2 )
				clif_bg_updatescore_single(sd);
		}

		if( map[sd->bl.m].flag.allowks && !map_flag_ks(sd->bl.m) )
		{
			char output[128];
			sprintf(output, "[ Kill Steal Protection Disable. KS is allowed in this map ]");
			clif_broadcast(&sd->bl, output, strlen(output) + 1, 0x10, SELF);
		}

		map_iwall_get(sd); // Updates Walls Info on this Map to Client
		sd->state.changemap = false;

		pc_setregstr(sd, add_str("@maploaded$"), map[sd->bl.m].name);
		npc_script_event(sd, NPCE_LOADMAP);
	}

	if( sd->state.changeregion )
	{
		if( index > 0 && battle_config.region_display == 1)
		{
			struct guild *g = guild_search(region[index].guild_id);
			char output[256];

			if( g == NULL )
				sprintf(output, "[ %s ]", region[index].name);
			else
				sprintf(output, "[ %s - Controled by %s ]", region[index].name, g->name);

			clif_broadcast2(&sd->bl, output, strlen(output) + 1, 0x00CCFF, 0x190, 12, 0, 0, SELF);

			if( g != NULL )
			{
				if( sd->status.guild_id && sd->status.guild_id == g->guild_id )
				{
					sprintf(output, "Guild controled territory [ + %d%% Exp | + %d%% Job ]", region[index].bexp, region[index].jexp);
					clif_disp_onlyself(sd, output, strlen(output));
				}
				else if( sd->status.guild_id && guild_isenemy(sd->status.guild_id, g->guild_id) )
				{
					sprintf(output, "Alert!! You are entering Enemy territory.");
					clif_disp_onlyself(sd, output, strlen(output));
				}
			}
		}

		sd->state.changeregion = false;
	}
	
#ifndef TXT_ONLY
	mail_clear(sd);
#endif

	if (pc_checkskill(sd, SG_DEVIL) && !pc_nextjobexp(sd))
		clif_status_load(&sd->bl, SI_DEVIL, 1);  //blindness [Komurka]

	if (sd->sc.opt2) //Client loses these on warp.
		clif_changeoption(&sd->bl);

	clif_weather_check(sd);
	
	// For automatic triggering of NPCs after map loading (so you don't need to walk 1 step first)
	if (map_getcell(sd->bl.m,sd->bl.x,sd->bl.y,CELL_CHKNPC))
		npc_touch_areanpc(sd,sd->bl.m,sd->bl.x,sd->bl.y);
	else
		sd->areanpc_id = 0;

  	// If player is dead, and is spawned (such as @refresh) send death packet. [Valaris]
	if(pc_isdead(sd))
		clif_clearunit_area(&sd->bl, CLR_DEAD);
// Uncomment if you want to make player face in the same direction he was facing right before warping. [Skotlex]
//	else
//		clif_changed_dir(&sd->bl, SELF);

//	Trigger skill effects if you appear standing on them
	if(!battle_config.pc_invincible_time)
		skill_unit_move(&sd->bl,gettick(),1);
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_TickSend(int fd, struct map_session_data *sd)
{
	sd->client_tick = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);

	WFIFOHEAD(fd, packet_len(0x7f));
	WFIFOW(fd,0)=0x7f;
	WFIFOL(fd,2)=gettick();
	WFIFOSET(fd,packet_len(0x7f));
	// removed until the socket problems are fixed. [FlavioJS]
	//flush_fifo(fd); // try to send immediatly so the client gets more accurate "pings"
	return;
}

void clif_hotkeys_send(struct map_session_data *sd) {
#ifdef HOTKEY_SAVING
	const int fd = sd->fd;
	int i;
#if PACKETVER<20090603
	const int cmd = 0x02b9;
#else
	const int cmd = 0x07d9;
#endif
	if (!fd) return;
	WFIFOHEAD(fd, 2+MAX_HOTKEYS*7);
	WFIFOW(fd, 0) = cmd;
	for(i = 0; i < MAX_HOTKEYS; i++) {
		WFIFOB(fd, 2 + 0 + i * 7) = sd->status.hotkeys[i].type; // type: 0: item, 1: skill
		WFIFOL(fd, 2 + 1 + i * 7) = sd->status.hotkeys[i].id; // item or skill ID
		WFIFOW(fd, 2 + 5 + i * 7) = sd->status.hotkeys[i].lv; // skill level
	}
	WFIFOSET(fd, packet_len(cmd));
#endif
}

void clif_parse_Hotkey(int fd, struct map_session_data *sd) {
#ifdef HOTKEY_SAVING
	unsigned short idx;
	int cmd;

	cmd = RFIFOW(fd, 0);
	idx = RFIFOW(fd, packet_db[sd->packet_ver][cmd].pos[0]);
	if (idx >= MAX_HOTKEYS) return;

	sd->status.hotkeys[idx].type = RFIFOB(fd, packet_db[sd->packet_ver][cmd].pos[1]);
	sd->status.hotkeys[idx].id = RFIFOL(fd, packet_db[sd->packet_ver][cmd].pos[2]);
	sd->status.hotkeys[idx].lv = RFIFOW(fd, packet_db[sd->packet_ver][cmd].pos[3]);
	return;
#endif
}

void clif_progressbar(struct map_session_data * sd, unsigned long color, unsigned int second)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x2f0));
	WFIFOW(fd,0) = 0x2f0;
	WFIFOL(fd,2) = color;
	WFIFOL(fd,6) = second;
	WFIFOSET(fd,packet_len(0x2f0));
}

void clif_progressbar_abort(struct map_session_data * sd)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x2f2));
	WFIFOW(fd,0) = 0x2f2;
	WFIFOSET(fd,packet_len(0x2f2));
}

void clif_parse_progressbar(int fd, struct map_session_data * sd)
{
	int npc_id = sd->progressbar.npc_id;

	if( gettick() < sd->progressbar.timeout && sd->st )
		sd->st->state = END;

	sd->progressbar.npc_id = sd->progressbar.timeout = 0;
	npc_scriptcont(sd, npc_id);
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_WalkToXY(int fd, struct map_session_data *sd)
{
	short x, y;
	int cmd;

	if (pc_isdead(sd)) {
		clif_clearunit_area(&sd->bl, CLR_DEAD);
		return;
	}

	if (sd->sc.opt1 && (sd->sc.opt1 == OPT1_STONEWAIT || sd->sc.opt1 == OPT1_BURNING))
		; //You CAN walk on this OPT1 value.
	else if( sd->progressbar.npc_id )
		clif_progressbar_abort(sd);
	else if (pc_cant_act(sd))
		return;

	if( sd->sc.data[SC_RUN] || sd->sc.data[SC_WUGDASH] )
		return;

	if( !pc_update_last_action(sd,2) )
		return;

	pc_delinvincibletimer(sd);

	cmd = RFIFOW(fd,0);
	x = RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0]) * 4 +
		(RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0] + 1) >> 6);
	y = ((RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0]+1) & 0x3f) << 4) +
		(RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0] + 2) >> 4);
	
	unit_walktoxy(&sd->bl, x, y, 4);
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_QuitGame(int fd, struct map_session_data *sd)
{
	WFIFOHEAD(fd,packet_len(0x18b));
	WFIFOW(fd,0) = 0x18b;

	/*	Rovert's prevent logout option fixed [Valaris]	*/
	if( !sd->sc.data[SC_CLOAKING] && !sd->sc.data[SC_HIDING] &&
		!sd->sc.data[SC_CHASEWALK] && !sd->sc.data[SC_CLOAKINGEXCEED] && !sd->sc.data[SC__INVISIBILITY] &&
		(!battle_config.prevent_logout || DIFF_TICK(gettick(), sd->canlog_tick) > battle_config.prevent_logout) )
	{
		set_eof(fd);
		WFIFOW(fd,2)=0;
	} else {
		WFIFOW(fd,2)=1;
	}
	WFIFOSET(fd,packet_len(0x18b));
}

/// Requesting unit's name
/// S 0094 <object id>.l
void clif_parse_GetCharNameRequest(int fd, struct map_session_data *sd)
{
	int id = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);
	struct block_list* bl;
	//struct status_change *sc;
	
	if( id < 0 && -id == sd->bl.id ) // for disguises [Valaris]
		id = sd->bl.id;

	bl = map_id2bl(id);
	if( bl == NULL )
		return;	// Lagged clients could request names of already gone mobs/players. [Skotlex]

	if( sd->bl.m != bl->m || !check_distance_bl(&sd->bl, bl, AREA_SIZE) )
		return; // Block namerequests past view range

	// 'see people in GM hide' cheat detection
	/* disabled due to false positives (network lag + request name of char that's about to hide = race condition)
	sc = status_get_sc(bl);
	if (sc && sc->option&OPTION_INVISIBLE && !disguised(bl) &&
		bl->type != BL_NPC && //Skip hidden NPCs which can be seen using Maya Purple
		pc_isGM(sd) < battle_config.hack_info_GM_level
	) {
		char gm_msg[256];
		sprintf(gm_msg, "Hack on NameRequest: character '%s' (account: %d) requested the name of an invisible target (id: %d).\n", sd->status.name, sd->status.account_id, id);
		ShowWarning(gm_msg);
		// information is sent to all online GMs
		intif_wis_message_to_gm(wisp_server_name, battle_config.hack_info_GM_level, gm_msg);
		return;
	}
	*/

	clif_charnameack(sd, bl);
}

/*==========================================
 * Validates and processes global messages
 * S 008c/00f3 <packet len>.w <text>.?B (<name> : <message>) 00
 *------------------------------------------*/
void clif_parse_GlobalMessage(int fd, struct map_session_data* sd)
{
	const char* text = (char*)RFIFOP(fd,4);
	int textlen = RFIFOW(fd,2) - 4;
	int tick = gettick();

	char *name, *message;
	int namelen, messagelen;

	// validate packet and retrieve name and message
	if( !clif_process_message(sd, 0, &name, &namelen, &message, &messagelen) )
		return;

	if( is_atcommand(fd, sd, message, 1)  )
		return;

	if( sd->sc.data[SC_BERSERK] || (sd->sc.data[SC_NOCHAT] && sd->sc.data[SC_NOCHAT]->val1&MANNER_NOCHAT) ||
		(sd->sc.data[SC_DEEPSLEEP] && sd->sc.data[SC_DEEPSLEEP]->val2) || sd->sc.data[SC_SATURDAYNIGHTFEVER])
		return;

	if( battle_config.min_chat_delay )
	{	//[Skotlex]
		if (DIFF_TICK(sd->cantalk_tick, tick) > 0)
			return;
		sd->cantalk_tick = tick + battle_config.min_chat_delay;
	}

	// [Flood Protection - Automute]
	if( battle_config.chat_allowed_per_interval && battle_config.chat_time_interval )
	{
		if( sd->last_talk_message == 0 )
			 sd->last_talk_message = tick;
		if( DIFF_TICK(tick, sd->last_talk_message) < 0 && sd->message_count >= battle_config.chat_allowed_per_interval )
		{ // Deny
			if( battle_config.chat_flood_automute )
			{
				sd->status.manner -= battle_config.chat_flood_automute;
				sc_start(&sd->bl,SC_NOCHAT,100,0,0); // Mute Player
			}
			return;
		}
		
		if( DIFF_TICK(tick, sd->last_talk_message) > 0 )
		{
			sd->message_count = 0;
			sd->last_talk_message = tick + battle_config.chat_time_interval * 1000;
		}
		sd->message_count++;
	}

	// send message to others (using the send buffer for temp. storage)
	WFIFOHEAD(fd, 8 + textlen);
	WFIFOW(fd,0) = 0x8d;
	WFIFOW(fd,2) = 8 + textlen;
	WFIFOL(fd,4) = sd->bl.id;
	safestrncpy((char*)WFIFOP(fd,8), text, textlen);
	//FIXME: chat has range of 9 only
	clif_send(WFIFOP(fd,0), WFIFOW(fd,2), &sd->bl, sd->chatID ? CHAT_WOS : AREA_CHAT_WOC);

	// send back message to the speaker
	memcpy(WFIFOP(fd,0), RFIFOP(fd,0), RFIFOW(fd,2));
	WFIFOW(fd,0) = 0x8e;
	WFIFOSET(fd, WFIFOW(fd,2));

#ifdef PCRE_SUPPORT
	// trigger listening npcs
	map_foreachinrange(npc_chat_sub, &sd->bl, AREA_SIZE, BL_NPC, text, textlen, &sd->bl);
#endif

	// Chat logging type 'O' / Global Chat
	if( log_config.chat&1 || (log_config.chat&2 && !(agit_flag && log_config.chat&64)) )
		log_chat("O", 0, sd->status.char_id, sd->status.account_id, mapindex_id2name(sd->mapindex), sd->bl.x, sd->bl.y, NULL, message);

	return;
}

/*==========================================
 * /mm /mapmove (as @rura GM command)
 *------------------------------------------*/
void clif_parse_MapMove(int fd, struct map_session_data *sd)
{
	char output[MAP_NAME_LENGTH_EXT+15]; // Max length of a short: ' -6XXXX' -> 7 digits
	char message[MAP_NAME_LENGTH_EXT+15+5]; // "/mm "+output
	char* map_name;

	if (battle_config.atc_gmonly && !pc_isGM(sd))
		return;
	if(pc_isGM(sd) < get_atcommand_level(atcommand_mapmove))
		return;

	map_name = (char*)RFIFOP(fd,2);
	map_name[MAP_NAME_LENGTH_EXT-1]='\0';
	sprintf(output, "%s %d %d", map_name, RFIFOW(fd,18), RFIFOW(fd,20));
	atcommand_mapmove(fd, sd, "@mapmove", output);
	if( log_config.gm && get_atcommand_level(atcommand_mapmove) >= log_config.gm )
	{
		sprintf(message, "/mm %s", output);
		log_atcommand(sd, message);
	}
	return;
}

/*==========================================
 *
 *------------------------------------------*/
void clif_changed_dir(struct block_list *bl, enum send_target target)
{
	unsigned char buf[64];

	WBUFW(buf,0) = 0x9c;
	WBUFL(buf,2) = bl->id;
	WBUFW(buf,6) = bl->type==BL_PC?((TBL_PC*)bl)->head_dir:0;
	WBUFB(buf,8) = unit_getdir(bl);

	clif_send(buf, packet_len(0x9c), bl, target);

	if (disguised(bl)) {
		WBUFL(buf,2) = -bl->id;
		WBUFW(buf,6) = 0;
		clif_send(buf, packet_len(0x9c), bl, SELF);
	}

	return;
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_ChangeDir(int fd, struct map_session_data *sd)
{
	unsigned char headdir, dir;

	headdir = RFIFOB(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);
	dir = RFIFOB(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]);
	pc_setdir(sd, dir, headdir);

	clif_changed_dir(&sd->bl, AREA_WOS);
	return;
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_Emotion(int fd, struct map_session_data *sd)
{
	int emoticon = RFIFOB(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);

	if (battle_config.basic_skill_check == 0 || pc_checkskill(sd, NV_BASIC) >= 2) {
		if (emoticon == E_MUTE) {// prevent use of the mute emote [Valaris]
			clif_skill_fail(sd, 1, 0, 1);
			return;
		}
		// fix flood of emotion icon (ro-proxy): flood only the hacker player
		if (sd->emotionlasttime >= time(NULL)) {
			sd->emotionlasttime = time(NULL) + 1; // not more than 1 per second (using /commands the client can spam it)
			clif_skill_fail(sd, 1, 0, 1);
			return;
		}
		sd->emotionlasttime = time(NULL) + 1; // not more than 1 per second (using /commands the client can spam it)

		if(battle_config.client_reshuffle_dice && emoticon>=E_DICE1 && emoticon<=E_DICE6)
		{// re-roll dice
			emoticon = rand()%6+E_DICE1;
		}

		clif_emotion(&sd->bl, emoticon);
	} else
		clif_skill_fail(sd, 1, 0, 1);
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_HowManyConnections(int fd, struct map_session_data *sd)
{
	WFIFOHEAD(fd,packet_len(0xc2));
	WFIFOW(fd,0) = 0xc2;
	WFIFOL(fd,2) = map_getusers();
	WFIFOSET(fd,packet_len(0xc2));
}

void clif_parse_ActionRequest_sub(struct map_session_data *sd, int action_type, int target_id, unsigned int tick)
{
	struct status_change *tsc = status_get_sc(map_id2bl(target_id));

	if (pc_isdead(sd)) {
		clif_clearunit_area(&sd->bl, CLR_DEAD);
		return;
	}

	if (sd->sc.count &&
		(sd->sc.data[SC_TRICKDEAD] ||
	 	sd->sc.data[SC_AUTOCOUNTER] ||
		sd->sc.data[SC_BLADESTOP] ||
	 	sd->sc.data[SC_DEATHBOUND] ||
		sd->sc.data[SC_CURSEDCIRCLE_ATKER] ||
		sd->sc.data[SC_CURSEDCIRCLE_TARGET]))
		return;

	pc_stop_walking(sd, 1);
	pc_stop_attack(sd);

	if(target_id<0 && -target_id == sd->bl.id) // for disguises [Valaris]
		target_id = sd->bl.id;

	switch(action_type)
	{
	case 0x00: // once attack
	case 0x07: // continuous attack

		if( pc_cant_act(sd) || sd->sc.option&OPTION_HIDE || sd->sc.data[SC_ALL_RIDING] )
			return;

		if( sd->sc.option&(OPTION_WEDDING|OPTION_XMAS|OPTION_SUMMER) )
			return;

		if( sd->sc.option&OPTION_RIDING_WUG && sd->weapontype1 )
			return;

		if( sd->sc.data[SC_BASILICA] || sd->sc.data[SC_CRYSTALIZE] || sd->sc.data[SC__SHADOWFORM] )
			return;

		if( (sd->sc.count && sd->sc.data[SC__MANHOLE]) || (tsc && tsc->data[SC__MANHOLE]) )
			return;

		if( sd->sc.data[SC_VOICEOFSIREN] && sd->sc.data[SC_VOICEOFSIREN]->val2 == target_id )
			return;

		if( sd->sc.data[SC_DEEPSLEEP] ) // Can't attack
			return;

		if (!battle_config.sdelay_attack_enable && pc_checkskill(sd, SA_FREECAST) <= 0) {
			if (DIFF_TICK(tick, sd->ud.canact_tick) < 0) {
				clif_skill_fail(sd, 1, 4, 0);
				return;
			}
		}

		if( !pc_update_last_action(sd,2) )
			return;

		pc_delinvincibletimer(sd);
		unit_attack(&sd->bl, target_id, action_type != 0);
	break;
	case 0x02: // sitdown
		if (battle_config.basic_skill_check && pc_checkskill(sd, NV_BASIC) < 3) {
			clif_skill_fail(sd, 1, 0, 2);
			break;
		}
		if( sd->sc.data[SC_SITDOWN_FORCE] )
			return;

		if(pc_issit(sd)) {
			//Bugged client? Just refresh them.
			clif_sitting(&sd->bl,false);
			return;
		}

		if (sd->ud.skilltimer != INVALID_TIMER || (sd->sc.opt1 && sd->sc.opt1 != OPT1_BURNING))
			break;

		if (sd->sc.count && (
			sd->sc.data[SC_DANCING] ||
			(sd->sc.data[SC_GRAVITATION] && sd->sc.data[SC_GRAVITATION]->val3 == BCT_SELF)
		)) //No sitting during these states either.
			break;

		if( !pc_update_last_action(sd,1) )
			break;

		pc_setsit(sd);
		skill_sit(sd,1);
		clif_sitting(&sd->bl,true);
	break;
	case 0x03: // standup
		if( sd->sc.data[SC_SITDOWN_FORCE] )
			return;

		if (!pc_issit(sd)) {
			//Bugged client? Just refresh them.
			clif_standing(&sd->bl,false);
			return;
		}

		if( !pc_update_last_action(sd,1) )
			break;

		pc_setstand(sd);
		skill_sit(sd,0); 
		clif_standing(&sd->bl,true);
	break;
	}
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_ActionRequest(int fd, struct map_session_data *sd)
{
	clif_parse_ActionRequest_sub(sd,
		RFIFOB(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]),
		RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]),
		gettick()
	);
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_Restart(int fd, struct map_session_data *sd)
{
	switch(RFIFOB(fd,2)) {
	case 0x00:
		pc_respawn(sd,CLR_RESPAWN);
		break;
	case 0x01:
		/*	Rovert's Prevent logout option - Fixed [Valaris]	*/
		if( !sd->sc.data[SC_CLOAKING] && !sd->sc.data[SC_HIDING] && !sd->sc.data[SC_CHASEWALK] &&
			!sd->sc.data[SC_CLOAKINGEXCEED] && !sd->sc.data[SC__INVISIBILITY] &&
			(!battle_config.prevent_logout || DIFF_TICK(gettick(), sd->canlog_tick) > battle_config.prevent_logout) )
		{	//Send to char-server for character selection.
			chrif_charselectreq(sd, session[fd]->client_addr);
		} else {
			WFIFOHEAD(fd,packet_len(0x18b));
			WFIFOW(fd,0)=0x18b;
			WFIFOW(fd,2)=1;

			WFIFOSET(fd,packet_len(0x018b));
		}
		break;
	}
}

/*==========================================
 * Validates and processes whispered messages
 * S 0096 <packet len>.w <nick>.24B <message>.?B
 *------------------------------------------*/
void clif_parse_WisMessage(int fd, struct map_session_data* sd)
{
	struct map_session_data* dstsd;
	char chatroom_symbol = '#';
	int i;

	char *target, *message;
	int namelen, messagelen;

	// validate packet and retrieve name and message
	if( !clif_process_message(sd, 1, &target, &namelen, &message, &messagelen) )
		return;

	if (is_atcommand(fd, sd, message, 1)  )
		return;

	if (sd->sc.data[SC_BERSERK] || (sd->sc.data[SC_NOCHAT] && sd->sc.data[SC_NOCHAT]->val1&MANNER_NOCHAT) ||
		(sd->sc.data[SC_DEEPSLEEP] && sd->sc.data[SC_DEEPSLEEP]->val2) || sd->sc.data[SC_SATURDAYNIGHTFEVER])
		return;

	if (battle_config.min_chat_delay)
	{	//[Skotlex]
		if (DIFF_TICK(sd->cantalk_tick, gettick()) > 0) {
			return;
		}
		sd->cantalk_tick = gettick() + battle_config.min_chat_delay;
	}

	// Chat logging type 'W' / Whisper
	if( log_config.chat&1 || (log_config.chat&4 && !(agit_flag && log_config.chat&64)) )
		log_chat("W", 0, sd->status.char_id, sd->status.account_id, mapindex_id2name(sd->mapindex), sd->bl.x, sd->bl.y, target, message);

	//-------------------------------------------------------//
	//   Lordalfa - Paperboy - To whisper NPC commands       //
	//-------------------------------------------------------//
	if (target[0] && (strncasecmp(target,"NPC:",4) == 0) && (strlen(target) > 4))
	{
		char* str = target+4; //Skip the NPC: string part.
		struct npc_data* npc;
		if ((npc = npc_name2id(str)))	
		{
			char split_data[10][50];
			char *split;
			char output[256];

			str = message;
			// skip codepage indicator, if detected
			if( str[0] == '|' && strlen(str) >= 4 )
				str += 3;
			for( i = 0; i < 10; ++i )
			{// Splits the message using '#' as separators
				split = strchr(str,'#');
				if( split == NULL )
				{	// use the remaining string
					safestrncpy(split_data[i], str, ARRAYLENGTH(split_data[i]));
					for( ++i; i < 10; ++i )
						split_data[i][0] = '\0';
					break;
				}
				*split = '\0';
				safestrncpy(split_data[i], str, ARRAYLENGTH(split_data[i]));
				str = split+1;
			}
			
			for( i = 0; i < 10; ++i )
			{
				sprintf(output, "@whispervar%d$", i);
				set_var(sd,output,(char *) split_data[i]);
			}
			
			sprintf(output, "%s::OnWhisperGlobal", npc->exname);
			npc_event(sd,output,0); // Calls the NPC label

			return;
		}
	}

	// Chat Channels [Zephyrus]
	if( battle_config.channel_system_enable && target[0] == chatroom_symbol )
	{
		if( battle_config.channel_min_chat_delay && DIFF_TICK(sd->channel_cantalk_tick, gettick()) > 0 )
			clif_displaymessage(sd->fd, msg_txt(876));
		else
		{
			channel_message(sd, target, message);
			sd->channel_cantalk_tick = gettick() + battle_config.channel_min_chat_delay;
		}
		return;
	}

	// searching destination character
	dstsd = map_nick2sd(target);

	if (dstsd == NULL || strcmp(dstsd->status.name, target) != 0)
	{
		// player is not on this map-server
		// At this point, don't send wisp/page if it's not exactly the same name, because (example)
		// if there are 'Test' player on an other map-server and 'test' player on this map-server,
		// and if we ask for 'Test', we must not contact 'test' player
		// so, we send information to inter-server, which is the only one which decide (and copy correct name).
		intif_wis_message(sd, target, message, messagelen);
		return;
	}
	
	// if player ignores everyone
	if (dstsd->state.ignoreAll)
	{
		if (dstsd->sc.option & OPTION_INVISIBLE && pc_isGM(sd) < pc_isGM(dstsd))
			clif_wis_end(fd, 1); // 1: target character is not loged in
		else
			clif_wis_end(fd, 3); // 3: everyone ignored by target
		return;
	}
	// if player ignores the source character
	ARR_FIND(0, MAX_IGNORE_LIST, i, dstsd->ignore[i].name[0] == '\0' || strcmp(dstsd->ignore[i].name, sd->status.name) == 0);
	if(i < MAX_IGNORE_LIST && dstsd->ignore[i].name[0] != '\0')
	{	// source char present in ignore list
		clif_wis_end(fd, 2); // 2: ignored by target
		return;
	}
	
	// if player is autotrading
	if( dstsd->state.autotrade == 1 )
	{
		char output[256];
		sprintf(output, "%s is in autotrade mode and cannot receive whispered messages.", dstsd->status.name);
		clif_wis_message(fd, wisp_server_name, output, strlen(output) + 1);
		return;
	}
	
	// notify sender of success
	clif_wis_end(fd, 0); // 0: success to send wisper

	// if player has an auto-away message
	if(dstsd->away_message[0] != '\0')
	{
		char output[256];
		sprintf(output, "%s %s", message, msg_txt(543)); // "(Automessage has been sent)"
		clif_wis_message(dstsd->fd, sd->status.name, output, strlen(output) + 1);
		if(dstsd->state.autotrade)
			sprintf(output, msg_txt(544), dstsd->away_message); // "Away [AT] - "%s""
		else
			sprintf(output, msg_txt(545), dstsd->away_message); // "Away - "%s""
		clif_wis_message(fd, dstsd->status.name, output, strlen(output) + 1);
		return;
	}
	// Normal message
	clif_wis_message(dstsd->fd, sd->status.name, message, messagelen);
	return;
}

/*==========================================
 * /b /nb
 * S 0099 <packet len>.w <text>.?B 00
 *------------------------------------------*/
void clif_parse_Broadcast(int fd, struct map_session_data* sd)
{
	char* msg = (char*)RFIFOP(fd,4);
	unsigned int len = RFIFOW(fd,2)-4;
	int lv;

	if( battle_config.atc_gmonly && !pc_isGM(sd) )
		return;
	if( pc_isGM(sd) < (lv=get_atcommand_level(atcommand_broadcast)) )
		return;

	// as the length varies depending on the command used, just block unreasonably long strings
	len = mes_len_check(msg, len, CHAT_SIZE_MAX);

	intif_broadcast(msg, len, 0);

	if(log_config.gm && lv >= log_config.gm) {
		char logmsg[CHAT_SIZE_MAX+4];
		sprintf(logmsg, "/b %s", msg);
		log_atcommand(sd, logmsg);
	}
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_TakeItem(int fd, struct map_session_data *sd)
{
	struct flooritem_data *fitem;
	int map_object_id;

	map_object_id = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);
	
	fitem = (struct flooritem_data*)map_id2bl(map_object_id);

	do {
		if (pc_isdead(sd)) {
			clif_clearunit_area(&sd->bl, CLR_DEAD);
			break;
		}

		if (fitem == NULL || fitem->bl.type != BL_ITEM || fitem->bl.m != sd->bl.m)
			break;
	
		if (pc_cant_act(sd))
			break;

		if(sd->sc.count && (
			sd->sc.data[SC_HIDING] ||
			sd->sc.data[SC_CLOAKING] ||
			sd->sc.data[SC_TRICKDEAD] ||
			sd->sc.data[SC_BLADESTOP] ||
			sd->sc.data[SC_CLOAKINGEXCEED] ||
			(sd->sc.data[SC_NOCHAT] && sd->sc.data[SC_NOCHAT]->val1&MANNER_NOITEM) ||
			sd->sc.data[SC_CURSEDCIRCLE_TARGET])
		)
			break;

		if (!pc_takeitem(sd, fitem))
			break;

		return;
	} while (0);
	// Client REQUIRES a fail packet or you can no longer pick items.
	clif_additem(sd,0,0,6);
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_DropItem(int fd, struct map_session_data *sd)
{
	int item_index = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0])-2;
	int item_amount = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]);

	for(;;) {
		if (pc_isdead(sd))
			break;

		if (pc_cant_act(sd))
			break;

		if (sd->sc.count && (
			sd->sc.data[SC_AUTOCOUNTER] ||
			sd->sc.data[SC_BLADESTOP] ||
			(sd->sc.data[SC_NOCHAT] && sd->sc.data[SC_NOCHAT]->val1&MANNER_NOITEM) ||
			sd->sc.data[SC_DEATHBOUND] ||
			sd->sc.data[SC_CURSEDCIRCLE_TARGET]
		))
			break;

		if (battle_config.super_woe_enable == 2 )
			break; // Cannot drop items on super woe servers

		if (!pc_dropitem(sd, item_index, item_amount))
			break;

		return;
	}

	//Because the client does not like being ignored.
	clif_dropitem(sd, item_index,0);
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_UseItem(int fd, struct map_session_data *sd)
{
	int n;

	if (pc_isdead(sd)) {
		clif_clearunit_area(&sd->bl, CLR_DEAD);
		return;
	}

	if (sd->sc.opt1 > 0 && sd->sc.opt1 != OPT1_STONEWAIT && sd->sc.opt1 != OPT1_BURNING)
		return;
	
	//This flag enables you to use items while in an NPC. [Skotlex]
	if (sd->npc_id) {
		if (sd->npc_id != sd->npc_item_flag)
			return;
	} else
	if (pc_istrading(sd))
		return;

	//Whether the item is used or not is irrelevant, the char ain't idle. [Skotlex]
	if( !pc_update_last_action(sd,1) )
		return;

	n = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0])-2;
	
	if(n <0 || n >= MAX_INVENTORY)
		return;
	if (!pc_useitem(sd,n))
		clif_useitemack(sd,n,0,0); //Send an empty ack packet or the client gets stuck.
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_EquipItem(int fd,struct map_session_data *sd)
{
	int index;

	if(pc_isdead(sd)) {
		clif_clearunit_area(&sd->bl,CLR_DEAD);
		return;
	}
	index = RFIFOW(fd,2)-2; 
	if (index < 0 || index >= MAX_INVENTORY)
		return; //Out of bounds check.
	
	if(sd->npc_id) {
		if (sd->npc_id != sd->npc_item_flag)
			return;
	} else if (sd->state.storage_flag || (sd->sc.opt1 && sd->sc.opt1 != OPT1_BURNING))
		; //You can equip/unequip stuff while storage is open/under status changes
	else if (pc_cant_act(sd))
		return;

	if(!sd->status.inventory[index].identify) {
		clif_equipitemack(sd,index,0,0);	// fail
		return;
	}

	if(!sd->inventory_data[index])
		return;

	if(sd->inventory_data[index]->type == IT_PETARMOR){
		pet_equipitem(sd,index);
		return;
	}
	
	//Client doesn't send the position for ammo.
	if( sd->inventory_data[index]->type == IT_AMMO ||
		sd->inventory_data[index]->type == IT_THROWWEAPON ||
		sd->inventory_data[index]->type == IT_CANNONBALL
	   )
		pc_equipitem(sd,index,EQP_AMMO);
	else
		pc_equipitem(sd,index,RFIFOW(fd,4));
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_UnequipItem(int fd,struct map_session_data *sd)
{
	int index;

	if(pc_isdead(sd)) {
		clif_clearunit_area(&sd->bl,CLR_DEAD);
		return;
	}

	if (sd->state.storage_flag)
		; //You can equip/unequip stuff while storage is open.
	else if (pc_cant_act(sd))
		return;

	index = RFIFOW(fd,2)-2;

	pc_unequipitem(sd,index,1);
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_NpcClicked(int fd,struct map_session_data *sd)
{
	struct block_list *bl;

	if(pc_isdead(sd)) {
		clif_clearunit_area(&sd->bl,CLR_DEAD);
		return;
	}

	if (pc_cant_act(sd))
		return;

	bl = map_id2bl(RFIFOL(fd,2));
	if (!bl) return;
	switch (bl->type) {
		case BL_MOB:
		case BL_PC:
			clif_parse_ActionRequest_sub(sd, 0x07, bl->id, gettick());
			break;
		case BL_NPC:
			if( bl->m != -1 )// the user can't click floating npcs directly (hack attempt)
				npc_click(sd,(TBL_NPC*)bl);
			break;
	}
	return;
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_NpcBuySellSelected(int fd,struct map_session_data *sd)
{
	if (sd->state.trading)
		return;
	npc_buysellsel(sd,RFIFOL(fd,2),RFIFOB(fd,6));
}

/// Request to buy chosen items from npc shop
/// S 00c8 <length>.w {<amount>.w <itemid>.w).4b*
/// R 00ca <result>.b
/// result = 00 -> "The deal has successfully completed." 
/// result = 01 -> "You do not have enough zeny."
/// result = 02 -> "You are over your Weight Limit."
/// result = 03 -> "Out of the maximum capacity, you have too many items."
void clif_parse_NpcBuyListSend(int fd, struct map_session_data* sd)
{
	int n = (RFIFOW(fd,2)-4) /4;
	unsigned short* item_list = (unsigned short*)RFIFOP(fd,4);
	int result;

	if( sd->state.trading || !sd->npc_shopid )
		result = 1;
	else if( sd->state.secure_items )
	{
		clif_displaymessage(sd->fd, "You can't buy. Blocked with @security");
		result = 1;
	}
	else
		result = npc_buylist(sd,n,item_list);

	sd->npc_shopid = 0; //Clear shop data.

	WFIFOHEAD(fd,packet_len(0xca));
	WFIFOW(fd,0) = 0xca;
	WFIFOB(fd,2) = result;
	WFIFOSET(fd,packet_len(0xca));
}

/// Request to sell chosen items to npc shop
/// R 00c9 <packet len>.W {<index>.W <amount>.W}.4B*
/// S 00cb <result>.B
/// result = 00 -> "The deal has successfully completed."
/// result = 01 -> "The deal has failed."
void clif_parse_NpcSellListSend(int fd,struct map_session_data *sd)
{
	int fail=0,n;
	unsigned short *item_list;

	n = (RFIFOW(fd,2)-4) /4;
	item_list = (unsigned short*)RFIFOP(fd,4);

	if( sd->state.trading || !sd->npc_shopid )
		fail = 1;
	else if( sd->state.secure_items )
	{
		clif_displaymessage(sd->fd, "You can't sell. Blocked with @security");
		fail = 1;
	}
	else
		fail = npc_selllist(sd,n,item_list);
	
	sd->npc_shopid = 0; //Clear shop data.

	WFIFOHEAD(fd,packet_len(0xcb));
	WFIFOW(fd,0)=0xcb;
	WFIFOB(fd,2)=fail;
	WFIFOSET(fd,packet_len(0xcb));
}

/*==========================================
 * Chatroom creation request
 * S 00d5 <len>.w <limit>.w <pub>.B <passwd>.8B <title>.?B
 *------------------------------------------*/
void clif_parse_CreateChatRoom(int fd, struct map_session_data* sd)
{
	int len = RFIFOW(fd,2)-15;
	int limit = RFIFOW(fd,4);
	bool pub = (RFIFOB(fd,6) != 0);
	const char* password = (char*)RFIFOP(fd,7); //not zero-terminated
	const char* title = (char*)RFIFOP(fd,15); // not zero-terminated
	char s_password[CHATROOM_PASS_SIZE];
	char s_title[CHATROOM_TITLE_SIZE];

	if (sd->sc.data[SC_NOCHAT] && sd->sc.data[SC_NOCHAT]->val1&MANNER_NOROOM)
		return;
	if(battle_config.basic_skill_check && pc_checkskill(sd,NV_BASIC) < 4) {
		clif_skill_fail(sd,1,0,3);
		return;
	}

	if( len <= 0 )
		return; // invalid input

	safestrncpy(s_password, password, CHATROOM_PASS_SIZE);
	safestrncpy(s_title, title, min(len+1,CHATROOM_TITLE_SIZE)); //NOTE: assumes that safestrncpy will not access the len+1'th byte

	chat_createpcchat(sd, s_title, s_password, limit, pub);
}

/*==========================================
 * Chatroom join request
 * S 00d9 <chat ID>.l <passwd>.8B
 *------------------------------------------*/
void clif_parse_ChatAddMember(int fd, struct map_session_data* sd)
{
	int chatid = RFIFOL(fd,2);
	const char* password = (char*)RFIFOP(fd,6); // not zero-terminated

	chat_joinchat(sd,chatid,password);
}

/*==========================================
 * Chatroom properties adjustment request
 * S 00de <len>.w <limit>.w <pub>.B <passwd>.8B <title>.?B
 *------------------------------------------*/
void clif_parse_ChatRoomStatusChange(int fd, struct map_session_data* sd)
{
	int len = RFIFOW(fd,2)-15;
	int limit = RFIFOW(fd,4);
	bool pub = (RFIFOB(fd,6) != 0);
	const char* password = (char*)RFIFOP(fd,7); // not zero-terminated
	const char* title = (char*)RFIFOP(fd,15); // not zero-terminated
	char s_password[CHATROOM_PASS_SIZE];
	char s_title[CHATROOM_TITLE_SIZE];

	if( len <= 0 )
		return; // invalid input

	safestrncpy(s_password, password, CHATROOM_PASS_SIZE);
	safestrncpy(s_title, title, min(len+1,CHATROOM_TITLE_SIZE)); //NOTE: assumes that safestrncpy will not access the len+1'th byte

	chat_changechatstatus(sd, s_title, s_password, limit, pub);
}

/*==========================================
 * CZ_REQ_ROLE_CHANGE
 * S 00e0 <role>.L <nick>.24B
 * role:
 *  0 = owner (ROOMROLE_OWNER)
 *  1 = normal (ROOMROLE_GENERAL)
 *------------------------------------------*/
void clif_parse_ChangeChatOwner(int fd, struct map_session_data* sd)
{
	chat_changechatowner(sd,(char*)RFIFOP(fd,6));
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_KickFromChat(int fd,struct map_session_data *sd)
{
	chat_kickchat(sd,(char*)RFIFOP(fd,2));
}

/*==========================================
 * Request to leave the current chatroom
 * S 00e3
 *------------------------------------------*/
void clif_parse_ChatLeave(int fd, struct map_session_data* sd)
{
	chat_leavechat(sd,0);
}

//Handles notifying asker and rejecter of what has just ocurred.
//Type is used to determine the correct msg_txt to use:
//0: 
static void clif_noask_sub(struct map_session_data *src, struct map_session_data *target, int type)
{
	const char* msg;
	char output[256];
	// Your request has been rejected by autoreject option.
	msg = msg_txt(392);
	clif_disp_onlyself(src, msg, strlen(msg));
	//Notice that a request was rejected.
	snprintf(output, 256, msg_txt(393+type), src->status.name, 256);
	clif_disp_onlyself(target, output, strlen(output));
}

/*==========================================
 * ����v���𑊎�ɑ���
 *------------------------------------------*/
void clif_parse_TradeRequest(int fd,struct map_session_data *sd)
{
	struct map_session_data *t_sd;
	
	t_sd = map_id2sd(RFIFOL(fd,2));

	if(!sd->chatID && pc_cant_act(sd))
		return; //You can trade while in a chatroom.

	// @noask [LuzZza]
	if(t_sd && t_sd->state.noask) {
		clif_noask_sub(sd, t_sd, 0);
		return;
	}

	if( battle_config.basic_skill_check && pc_checkskill(sd,NV_BASIC) < 1)
	{
		clif_skill_fail(sd,1,0,0);
		return;
	}

	if( ( battle_config.super_woe_enable == 2 || ( battle_config.super_woe_enable == 1 && t_sd && t_sd->status.guild_id != sd->status.guild_id ) ) && !(pc_isGM(sd) || pc_isGM(t_sd)) )
	{
		clif_displaymessage(fd, "- Event Server * Cannot Trade items to other guild members -");
		return;
	}
	
	trade_traderequest(sd,t_sd);
}

/*==========================================
 * ����v��
 *------------------------------------------*/
void clif_parse_TradeAck(int fd,struct map_session_data *sd)
{
	trade_tradeack(sd,RFIFOB(fd,2));
}

/*==========================================
 * �A�C�e���ǉ�
 *------------------------------------------*/
void clif_parse_TradeAddItem(int fd,struct map_session_data *sd)
{
	short index = RFIFOW(fd,2);
	int amount = RFIFOL(fd,4);

	if( index == 0 )
		trade_tradeaddzeny(sd, amount);
	else
		trade_tradeadditem(sd, index, (short)amount);
}

/*==========================================
 * �A�C�e���ǉ�����(ok����)
 *------------------------------------------*/
void clif_parse_TradeOk(int fd,struct map_session_data *sd)
{
	trade_tradeok(sd);
}

/*==========================================
 * ����L�����Z��
 *------------------------------------------*/
void clif_parse_TradeCancel(int fd,struct map_session_data *sd)
{
	trade_tradecancel(sd);
}

/*==========================================
 * �������(trade����)
 *------------------------------------------*/
void clif_parse_TradeCommit(int fd,struct map_session_data *sd)
{
	trade_tradecommit(sd);
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_StopAttack(int fd,struct map_session_data *sd)
{
	pc_stop_attack(sd);
}

/*==========================================
 * �J�[�g�փA�C�e�����ڂ�
 *------------------------------------------*/
void clif_parse_PutItemToCart(int fd,struct map_session_data *sd)
{
	if (pc_istrading(sd))
		return;
	if (!pc_iscarton(sd))
		return;
	pc_putitemtocart(sd,RFIFOW(fd,2)-2,RFIFOL(fd,4));
	pc_update_last_action(sd,0);
}
/*==========================================
 * �J�[�g����A�C�e�����o��
 *------------------------------------------*/
void clif_parse_GetItemFromCart(int fd,struct map_session_data *sd)
{
	if (!pc_iscarton(sd))
		return;
	pc_getitemfromcart(sd,RFIFOW(fd,2)-2,RFIFOL(fd,4));
	pc_update_last_action(sd,0);
}

/*==========================================
 * �t���i(��,�y�R,�J�[�g)���͂���
 *------------------------------------------*/
void clif_parse_RemoveOption(int fd,struct map_session_data *sd)
{
	//Can only remove Cart/Riding Peco/Falcon/Riding Dragon/Mado.
	pc_setoption(sd,sd->sc.option&~(OPTION_CART|OPTION_RIDING|OPTION_FALCON|(OPTION_RIDING_DRAGON)|OPTION_MADO));
}

/*==========================================
 * �`�F���W�J�[�g
 *------------------------------------------*/
void clif_parse_ChangeCart(int fd,struct map_session_data *sd)
{
	int type;

	if( sd && pc_checkskill(sd, MC_CHANGECART) < 1 )
		return;

	type = (int)RFIFOW(fd,2);

	if( (type == 5 && sd->status.base_level > 90) ||
	    (type == 4 && sd->status.base_level > 80) ||
	    (type == 3 && sd->status.base_level > 65) ||
	    (type == 2 && sd->status.base_level > 40) ||
	    (type == 1))
		pc_setcart(sd,type);
}

/*==========================================
 * �X�e�[�^�X�A�b�v
 *------------------------------------------*/
void clif_parse_StatusUp(int fd,struct map_session_data *sd)
{
	pc_statusup(sd,RFIFOW(fd,2));
}

/*==========================================
 * �X�L�����x���A�b�v
 *------------------------------------------*/
void clif_parse_SkillUp(int fd,struct map_session_data *sd)
{
	pc_skillup(sd,RFIFOW(fd,2));
}

static void clif_parse_UseSkillToId_homun(struct homun_data *hd, struct map_session_data *sd, unsigned int tick, short skillnum, short skilllv, int target_id)
{
	int lv;

	if( !hd )
		return;
	if( skillnotok_hom(skillnum, hd) )
		return;
	if( hd->bl.id != target_id && skill_get_inf(skillnum)&INF_SELF_SKILL )
		target_id = hd->bl.id;
	if( hd->ud.skilltimer != INVALID_TIMER )
	{
		if( skillnum != SA_CASTCANCEL ) return;
	}
	else if( DIFF_TICK(tick, hd->ud.canact_tick) < 0 )
		return;

	lv = merc_hom_checkskill(hd, skillnum);
	if( skilllv > lv )
		skilllv = lv;
	if( skilllv )
		unit_skilluse_id(&hd->bl, target_id, skillnum, skilllv);
}

static void clif_parse_UseSkillToId_mercenary(struct mercenary_data *md, struct map_session_data *sd, unsigned int tick, short skillnum, short skilllv, int target_id)
{
	int lv;

	if( !md )
		return;
	if( skillnotok_mercenary(skillnum, md) )
		return;
	if( md->bl.id != target_id && skill_get_inf(skillnum)&INF_SELF_SKILL )
		target_id = md->bl.id;
	if( md->ud.skilltimer != INVALID_TIMER )
	{
		if( skillnum != SA_CASTCANCEL ) return;
	}
	else if( DIFF_TICK(tick, md->ud.canact_tick) < 0 )
		return;

	lv = mercenary_checkskill(md, skillnum);
	if( skilllv > lv )
		skilllv = lv;
	if( skilllv )
		unit_skilluse_id(&md->bl, target_id, skillnum, skilllv);
}

static void clif_parse_UseSkillToPos_mercenary(struct mercenary_data *md, struct map_session_data *sd, unsigned int tick, short skillnum, short skilllv, short x, short y, int skillmoreinfo)
{
	int lv;
	if( !md )
		return;
	if( skillnotok_mercenary(skillnum, md) )
		return;
	if( md->ud.skilltimer != INVALID_TIMER )
		return;
	if( DIFF_TICK(tick, md->ud.canact_tick) < 0 )
	{
		clif_skill_fail(md->master, skillnum, 4, 0);
		return;
	}

	if( md->sc.data[SC_BASILICA] )
		return;
	lv = mercenary_checkskill(md, skillnum);
	if( skilllv > lv )
		skilllv = lv;
	if( skilllv )
		unit_skilluse_pos(&md->bl, x, y, skillnum, skilllv);
}

/*==========================================
 * �X�L���g�p�iID�w��j
 *------------------------------------------*/
void clif_parse_UseSkillToId(int fd, struct map_session_data *sd)
{
	short skillnum, skilllv;
	int tmp, target_id;
	unsigned int tick = gettick();

	skilllv = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);
	skillnum = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]);
	target_id = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[2]);

	if( skilllv < 1 ) skilllv = 1; //No clue, I have seen the client do this with guild skills :/ [Skotlex]

	tmp = skill_get_inf(skillnum);
	if (tmp&INF_GROUND_SKILL || !tmp)
		return; //Using a ground/passive skill on a target? WRONG.

	if( skillnum >= HM_SKILLBASE && skillnum < HM_SKILLBASE + MAX_HOMUNSKILL )
	{
		clif_parse_UseSkillToId_homun(sd->hd, sd, tick, skillnum, skilllv, target_id);
		return;
	}

	if( skillnum >= MC_SKILLBASE && skillnum < MC_SKILLBASE + MAX_MERCSKILL )
	{
		clif_parse_UseSkillToId_mercenary(sd->md, sd, tick, skillnum, skilllv, target_id);
		return;
	}

	if( sd->bl.id != target_id && tmp&INF_SELF_SKILL )
		target_id = sd->bl.id; // never trust the client

	if( target_id < 0 && -target_id == sd->bl.id ) // for disguises [Valaris]
		target_id = sd->bl.id;

	// Whether skill fails or not is irrelevant, the char ain't idle. [Skotlex]
	if( !pc_update_last_action(sd,1 + (tmp&INF_SELF_SKILL ? 0 : 2)) )
		return;

	if( pc_cant_act(sd) && skillnum != SR_GENTLETOUCH_CURE )
		return;
	if( pc_issit(sd) )
		return;

	if( skillnotok(skillnum, sd) )
		return;

	if( sd->sc.data[SC_VOICEOFSIREN] && sd->sc.data[SC_VOICEOFSIREN]->val2 == target_id )
		return;
	
	if( sd->ud.skilltimer != INVALID_TIMER )
	{
		if( skillnum != SA_CASTCANCEL &&
			!(skillnum == SO_SPELLFIST && (sd->ud.skillid == MG_FIREBOLT || sd->ud.skillid == MG_COLDBOLT || sd->ud.skillid == MG_LIGHTNINGBOLT)) )
			return;
	}
	else if( DIFF_TICK(tick, sd->ud.canact_tick) < 0 )
	{
		if( sd->skillitem != skillnum )
		{
			clif_skill_fail(sd, skillnum, 4, 0);
			return;
		}
	}

	if( sd->sc.option&(OPTION_WEDDING|OPTION_XMAS|OPTION_SUMMER) )
		return;

	if( sd->sc.data[SC_BASILICA] && (skillnum != HP_BASILICA || sd->sc.data[SC_BASILICA]->val4 != sd->bl.id) )
		return; // On basilica only caster can use Basilica again to stop it.

	if( sd->sc.data[SC__MANHOLE] )
		return;

	if( sd->menuskill_id )
	{
		if( sd->menuskill_id == SA_TAMINGMONSTER )
			sd->menuskill_id = sd->menuskill_val = 0; //Cancel pet capture.
		else if( sd->menuskill_id != SA_AUTOSPELL )
			return; //Can't use skills while a menu is open.
	}
	if( sd->skillitem == skillnum )
	{
		if( skilllv != sd->skillitemlv )
			skilllv = sd->skillitemlv;
		if( !(tmp&INF_SELF_SKILL) )
			pc_delinvincibletimer(sd); // Target skills thru items cancel invincibility. [Inkfish]
		unit_skilluse_id(&sd->bl, target_id, skillnum, skilllv);
		return;
	}

	sd->skillitem = sd->skillitemlv = 0;

	if( skillnum >= GD_SKILLBASE )
	{
		int idx = battle_config.guild_skills_separed_delay ? skillnum - GD_SKILLBASE : 0;
		if( idx < 0 || idx >= MAX_GUILDSKILL )
			skilllv = 0;
		else if( !map[sd->bl.m].flag.battleground )
		{
			struct guild *g;
			if( (g = sd->state.gmaster_flag) != NULL )
			{
				if( g->skill_block_timer[idx] == INVALID_TIMER )
					skilllv = guild_checkskill(g, skillnum);
				else
				{
					guild_block_skill_status(g, skillnum);
					skilllv = 0;
				}
			}
			else
				skilllv = 0;
		}
		else
		{
			struct battleground_data *bg;
			if( (bg = sd->bmaster_flag) != NULL )
			{
				if( bg->skill_block_timer[idx] == INVALID_TIMER )
					skilllv = bg_checkskill(bg, skillnum);
				else
				{
					bg_block_skill_status(bg, skillnum);
					skilllv = 0;
				}
			}
			else
				skilllv = 0;
		}
	}
	else
	{
		tmp = pc_checkskill(sd, skillnum);
		if( skilllv > tmp )
			skilllv = tmp;
	}

	pc_delinvincibletimer(sd);
	
	if( skilllv )
		unit_skilluse_id(&sd->bl, target_id, skillnum, skilllv);
}

/*==========================================
 * �X�L���g�p�i�ꏊ�w��j
 *------------------------------------------*/
void clif_parse_UseSkillToPosSub(int fd, struct map_session_data *sd, short skilllv, short skillnum, short x, short y, int skillmoreinfo)
{
	int lv;
	unsigned int tick = gettick();

	if( !(skill_get_inf(skillnum)&INF_GROUND_SKILL) )
		return; //Using a target skill on the ground? WRONG.

	if( skillnum >= MC_SKILLBASE && skillnum < MC_SKILLBASE + MAX_MERCSKILL )
	{
		clif_parse_UseSkillToPos_mercenary(sd->md, sd, tick, skillnum, skilllv, x, y, skillmoreinfo);
		return;
	}

	//Whether skill fails or not is irrelevant, the char ain't idle. [Skotlex]
	if( !pc_update_last_action(sd,3) )
		return;

	if( skillnotok(skillnum, sd) )
		return;
	if( skillmoreinfo != -1 )
	{
		if( pc_issit(sd) )
		{
			clif_skill_fail(sd, skillnum, 0, 0);
			return;
		}
		//You can't use Graffiti/TalkieBox AND have a vending open, so this is safe.
		safestrncpy(sd->message, (char*)RFIFOP(fd,skillmoreinfo), MESSAGE_SIZE);
	}

	if( sd->ud.skilltimer != INVALID_TIMER )
		return;

	if( DIFF_TICK(tick, sd->ud.canact_tick) < 0 )
	{
		if( sd->skillitem != skillnum )
		{
			clif_skill_fail(sd, skillnum, 4, 0);
			return;
		}
	}

	if( sd->sc.option&(OPTION_WEDDING|OPTION_XMAS|OPTION_SUMMER) )
		return;

	if( sd->sc.data[SC_BASILICA] && (skillnum != HP_BASILICA || sd->sc.data[SC_BASILICA]->val4 != sd->bl.id) )
		return; // On basilica only caster can use Basilica again to stop it.
	
	if( sd->sc.data[SC__MANHOLE] )
		return;

	if( sd->menuskill_id )
	{
		if( sd->menuskill_id == SA_TAMINGMONSTER )
			sd->menuskill_id = sd->menuskill_val = 0; //Cancel pet capture.
		else if( sd->menuskill_id != SA_AUTOSPELL )
			return; //Can't use skills while a menu is open.
	}

	pc_delinvincibletimer(sd);

	if( sd->skillitem == skillnum )
	{
		if( skilllv != sd->skillitemlv )
			skilllv = sd->skillitemlv;
		unit_skilluse_pos(&sd->bl, x, y, skillnum, skilllv);
	}
	else
	{
		sd->skillitem = sd->skillitemlv = 0;
		if( (lv = pc_checkskill(sd, skillnum)) > 0 )
		{
			if( skilllv > lv )
				skilllv = lv;
			unit_skilluse_pos(&sd->bl, x, y, skillnum,skilllv);
		}
	}
}

void clif_parse_UseSkillToPos(int fd, struct map_session_data *sd)
{
	if (pc_cant_act(sd))
		return;
	if (pc_issit(sd))
		return;

	clif_parse_UseSkillToPosSub(fd, sd,
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]), //skill lv
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]), //skill num
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[2]), //pos x
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[3]), //pos y
		-1	//Skill more info.
	);
}

void clif_parse_UseSkillToPosMoreInfo(int fd, struct map_session_data *sd)
{
	if (pc_cant_act(sd))
		return;
	if (pc_issit(sd))
		return;
	
	clif_parse_UseSkillToPosSub(fd, sd,
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]), //Skill lv
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]), //Skill num
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[2]), //pos x
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[3]), //pos y
		packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[4] //skill more info
	);
}
/*==========================================
 * �X�L���g�p�imap�w��j
 *------------------------------------------*/
void clif_parse_UseSkillMap(int fd, struct map_session_data* sd)
{
	short skill_num = RFIFOW(fd,2);
	char map_name[MAP_NAME_LENGTH];
	mapindex_getmapname((char*)RFIFOP(fd,4), map_name);

	if(skill_num != sd->menuskill_id) 
		return;

	if( sd->sc.data[SC__MANHOLE] )
		return;

	if( pc_cant_act(sd) )
	{
		sd->menuskill_id = sd->menuskill_val = 0;
		return;
	}

	pc_delinvincibletimer(sd);
	skill_castend_map(sd,skill_num,map_name);
}
/*==========================================
 * �����v��
 *------------------------------------------*/
void clif_parse_RequestMemo(int fd,struct map_session_data *sd)
{
	if (!pc_isdead(sd))
		pc_memo(sd,-1);
}
/*==========================================
 * �A�C�e������
 *------------------------------------------*/
void clif_parse_ProduceMix(int fd,struct map_session_data *sd)
{
	// -1 is used by produce script command.
	if( sd->menuskill_id != -1 && sd->menuskill_id != AM_PHARMACY && sd->menuskill_id != SA_CREATECON &&
		sd->menuskill_id != RK_RUNEMASTERY && sd->menuskill_id != GC_CREATENEWPOISON )
		return;

	if (pc_istrading(sd)) {
		//Make it fail to avoid shop exploits where you sell something different than you see.
		clif_skill_fail(sd,sd->ud.skillid,0,0);
		sd->menuskill_val = sd->menuskill_id = sd->menuskill_itemused = 0;
		return;
	}
	skill_produce_mix(sd,0,RFIFOW(fd,2),RFIFOW(fd,4),RFIFOW(fd,6),RFIFOW(fd,8), 1);
	sd->menuskill_val = sd->menuskill_id = sd->menuskill_itemused = 0;
}

/*==========================================
 * To SC_AUTOSHADOWSPELL
 *------------------------------------------*/
void clif_parse_SkillSelectMenu(int fd, struct map_session_data *sd)
{
	if( sd->menuskill_id != SC_AUTOSHADOWSPELL )
		return;

	if( pc_istrading(sd) )
	{
		clif_skill_fail(sd,sd->ud.skillid,0,0);
		sd->menuskill_val = sd->menuskill_id = 0;
		return;
	}
	skill_select_menu(sd,RFIFOL(fd,2),RFIFOW(fd,6));
	sd->menuskill_val = sd->menuskill_id = 0;
}
/*==========================================
 *
 *------------------------------------------*/
void clif_parse_Cooking(int fd,struct map_session_data *sd)
{
	int type = RFIFOW(fd,2);
	int nameid = RFIFOW(fd,4);

	if( type == 6 && sd->menuskill_id != GN_MIX_COOKING && sd->menuskill_id != GN_S_PHARMACY )
		return;

	if (pc_istrading(sd)) {
		//Make it fail to avoid shop exploits where you sell something different than you see.
		clif_skill_fail(sd,sd->ud.skillid,0,0);
		sd->menuskill_val = sd->menuskill_id = 0;
		return;
	}
	skill_produce_mix(sd,sd->menuskill_id,nameid,0,0,0,sd->menuskill_itemused);
	sd->menuskill_val = sd->menuskill_id = sd->menuskill_itemused = 0;
}
/*==========================================
 * ����C��
 *------------------------------------------*/
void clif_parse_RepairItem(int fd, struct map_session_data *sd)
{
	if (sd->menuskill_id != BS_REPAIRWEAPON)
		return;
	if (pc_istrading(sd)) {
		//Make it fail to avoid shop exploits where you sell something different than you see.
		clif_skill_fail(sd,sd->ud.skillid,0,0);
		sd->menuskill_val = sd->menuskill_id = 0;
		return;
	}
	skill_repairweapon(sd,RFIFOW(fd,2));
	sd->menuskill_val = sd->menuskill_id = 0;
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_WeaponRefine(int fd, struct map_session_data *sd)
{
	int idx;

	if (sd->menuskill_id != WS_WEAPONREFINE) //Packet exploit?
		return;
	if (pc_istrading(sd) || battle_config.super_woe_enable) {
		//Make it fail to avoid shop exploits where you sell something different than you see.
		clif_skill_fail(sd,sd->ud.skillid,0,0);
		sd->menuskill_val = sd->menuskill_id = 0;
		return;
	}
	idx = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);
	skill_weaponrefine(sd, idx-2);
	sd->menuskill_val = sd->menuskill_id = 0;
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_NpcSelectMenu(int fd,struct map_session_data *sd)
{
	int npc_id = RFIFOL(fd,2);
	uint8 select = RFIFOB(fd,6);

	if( (select > sd->npc_menu && select != 0xff) || select == 0 )
	{
		TBL_NPC* nd = map_id2nd(npc_id);
		ShowWarning("Invalid menu selection on npc %d:'%s' - got %d, valid range is [%d..%d] (player AID:%d, CID:%d, name:'%s')!\n", npc_id, (nd)?nd->name:"invalid npc id", select, 1, sd->npc_menu, sd->bl.id, sd->status.char_id, sd->status.name);
		clif_GM_kick(NULL,sd);
		return;
	}

	sd->npc_menu = select;
	npc_scriptcont(sd,npc_id);
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_NpcNextClicked(int fd,struct map_session_data *sd)
{
	npc_scriptcont(sd,RFIFOL(fd,2));
}

/*==========================================
 * Value entered into a NPC input box.
 * S 0143 <npcID>.l <amount>.l
 *------------------------------------------*/
void clif_parse_NpcAmountInput(int fd,struct map_session_data *sd)
{
	int npcid = RFIFOL(fd,2);
	int amount = (int)RFIFOL(fd,6);

	sd->npc_amount = amount;
	npc_scriptcont(sd, npcid);
}

/*==========================================
 * Text entered into a NPC input box.
 * S 01d5 <len>.w <npcID>.l <input>.?B 00
 *------------------------------------------*/
void clif_parse_NpcStringInput(int fd, struct map_session_data* sd)
{
	int message_len = RFIFOW(fd,2)-8;
	int npcid = RFIFOL(fd,4);
	const char* message = (char*)RFIFOP(fd,8);
	
	if( message_len <= 0 )
		return; // invalid input

	safestrncpy(sd->npc_str, message, min(message_len,CHATBOX_SIZE));
	npc_scriptcont(sd, npcid);
}

/*==========================================
 *
 *------------------------------------------*/
void clif_parse_NpcCloseClicked(int fd,struct map_session_data *sd)
{
	if (!sd->npc_id) //Avoid parsing anything when the script was done with. [Skotlex]
		return; 
	npc_scriptcont(sd,RFIFOL(fd,2));
}

/*==========================================
 * �A�C�e���Ӓ�
 *------------------------------------------*/
void clif_parse_ItemIdentify(int fd,struct map_session_data *sd)
{
	short idx = RFIFOW(fd,2);

	if (sd->menuskill_id != MC_IDENTIFY)
		return;
	if( idx == -1 )
	{// cancel pressed
		sd->menuskill_val = sd->menuskill_id = 0;
		return;
	}
	skill_identify(sd,idx-2);
	sd->menuskill_val = sd->menuskill_id = 0;
}
/*==========================================
 * ��쐬
 *------------------------------------------*/
void clif_parse_SelectArrow(int fd,struct map_session_data *sd)
{
	switch( sd->menuskill_id )
	{
	case AC_MAKINGARROW:
	case WL_READING_SB:
	case GC_POISONINGWEAPON:
	case NC_MAGICDECOY:
	case MC_VENDING: // Extended Vending System
		break;
	default:
		return;
	}

	if (pc_istrading(sd)) {
	//Make it fail to avoid shop exploits where you sell something different than you see.
		clif_skill_fail(sd,sd->ud.skillid,0,0);
		sd->menuskill_val = sd->menuskill_id = 0;
		return;
	}

	switch( sd->menuskill_id )
	{
	case AC_MAKINGARROW:
		skill_arrow_create(sd,RFIFOW(fd,2));
		break;
	case MC_VENDING: // Extended Vending System
		skill_vending(sd,RFIFOW(fd,2));
		break;
	case WL_READING_SB:
		skill_spellbook(sd,RFIFOW(fd,2));
		break;
	case GC_POISONINGWEAPON:
		skill_poisoningweapon(sd,RFIFOW(fd,2));
		break;
	case NC_MAGICDECOY:
		skill_magicdecoy(sd,RFIFOW(fd,2));
		break;
	}

	sd->menuskill_val = sd->menuskill_id = 0;
}
/*==========================================
 * �I�[�g�X�y����M
 *------------------------------------------*/
void clif_parse_AutoSpell(int fd,struct map_session_data *sd)
{
	if (sd->menuskill_id != SA_AUTOSPELL)
		return;
	skill_autospell(sd,RFIFOW(fd,2));
	sd->menuskill_val = sd->menuskill_id = 0;
}
/*==========================================
 * �J�[�h�g�p
 *------------------------------------------*/
void clif_parse_UseCard(int fd,struct map_session_data *sd)
{
	if (sd->state.trading != 0)
		return;
	clif_use_card(sd,RFIFOW(fd,2)-2);
}
/*==========================================
 * �J�[�h�}�������I��
 *------------------------------------------*/
void clif_parse_InsertCard(int fd,struct map_session_data *sd)
{
	if (sd->state.trading != 0)
		return;
	pc_insert_card(sd,RFIFOW(fd,2)-2,RFIFOW(fd,4)-2);
}

/*==========================================
 * 0193 �L����ID���O����
 *------------------------------------------*/
void clif_parse_SolveCharName(int fd, struct map_session_data *sd)
{
	int charid;

	charid = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);
	map_reqnickdb(sd, charid);
}

/*==========================================
 * 0197 /resetskill /resetstate
 *------------------------------------------*/
void clif_parse_ResetChar(int fd, struct map_session_data *sd)
{
	if( battle_config.atc_gmonly && !pc_isGM(sd) )
		return;

	if( pc_isGM(sd) < get_atcommand_level(atcommand_reset) )
		return;

	if( RFIFOW(fd,2) )
		pc_resetskill(sd,1);
	else
		pc_resetstate(sd);

	if( log_config.gm && get_atcommand_level(atcommand_reset) >= log_config.gm )
		log_atcommand(sd, RFIFOW(fd,2) ? "/resetskill" : "/resetstate");
}

/*==========================================
 * /lb /nlb
 * S 019c <packet len>.w <text>.?B 00
 *------------------------------------------*/
void clif_parse_LocalBroadcast(int fd, struct map_session_data* sd)
{
	char* msg = (char*)RFIFOP(fd,4);
	unsigned int len = RFIFOW(fd,2)-4;
	int lv;

	if( battle_config.atc_gmonly && !pc_isGM(sd) )
		return;

	if( pc_isGM(sd) < (lv=get_atcommand_level(atcommand_localbroadcast)) )
		return;

	// as the length varies depending on the command used, just block unreasonably long strings
	len = mes_len_check(msg, len, CHAT_SIZE_MAX);

	clif_broadcast(&sd->bl, msg, len, 0, ALL_SAMEMAP);

	if( log_config.gm && lv >= log_config.gm ) {
		char logmsg[CHAT_SIZE_MAX+5];
		sprintf(logmsg, "/lb %s", msg);
		log_atcommand(sd, logmsg);
	}
}

/*==========================================
 * �J�v���q�ɂ֓����
 *------------------------------------------*/
void clif_parse_MoveToKafra(int fd, struct map_session_data *sd)
{
	int item_index, item_amount;

	if (pc_istrading(sd))
		return;
	
	item_index = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0])-2;
	item_amount = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]);
	if (item_index < 0 || item_index >= MAX_INVENTORY || item_amount < 1)
		return;

	if (sd->state.storage_flag == 1)
		storage_storageadd(sd, item_index, item_amount);
	else
	if (sd->state.storage_flag == 2)
		storage_guild_storageadd(sd, item_index, item_amount);
	else
	if (sd->state.storage_flag == 3)
		ext_storage_add(sd, item_index, item_amount);
	else return;
	pc_update_last_action(sd,0);
}

/*==========================================
 * �J�v���q�ɂ���o��
 *------------------------------------------*/
void clif_parse_MoveFromKafra(int fd,struct map_session_data *sd)
{
	int item_index, item_amount;

	item_index = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0])-1;
	item_amount = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]);

	if (sd->state.storage_flag == 1)
		storage_storageget(sd, item_index, item_amount);
	else
	if(sd->state.storage_flag == 2)
		storage_guild_storageget(sd, item_index, item_amount);
	else
	if (sd->state.storage_flag == 3)
		ext_storage_get(sd, item_index, item_amount);
	else return;
	pc_update_last_action(sd,0);
}

/*==========================================
 * �J�v���q�ɂփJ�[�g��������
 *------------------------------------------*/
void clif_parse_MoveToKafraFromCart(int fd, struct map_session_data *sd)
{
	if( sd->state.vending )
		return;
	if (!pc_iscarton(sd))
		return;

	if (sd->state.storage_flag == 1)
		storage_storageaddfromcart(sd, RFIFOW(fd,2) - 2, RFIFOL(fd,4));
	else
	if (sd->state.storage_flag == 2)
		storage_guild_storageaddfromcart(sd, RFIFOW(fd,2) - 2, RFIFOL(fd,4));
	else
	if(sd->state.storage_flag == 3)
		ext_storage_addfromcart(sd, RFIFOW(fd,2) - 2, RFIFOL(fd,4));
	else return;
	pc_update_last_action(sd,0);
}

/*==========================================
 * �J�v���q�ɂ���o��
 *------------------------------------------*/
void clif_parse_MoveFromKafraToCart(int fd, struct map_session_data *sd)
{
	if( sd->state.vending )
		return;
	if (!pc_iscarton(sd))
		return;

	if (sd->state.storage_flag == 1)
		storage_storagegettocart(sd, RFIFOW(fd,2)-1, RFIFOL(fd,4));
	else
	if (sd->state.storage_flag == 2)
		storage_guild_storagegettocart(sd, RFIFOW(fd,2)-1, RFIFOL(fd,4));
	else
	if (sd->state.storage_flag == 3)
		ext_storage_gettocart(sd, RFIFOW(fd,2)-1, RFIFOL(fd,4));
	else return;
	pc_update_last_action(sd,0);
}

/*==========================================
 * �J�v���q�ɂ����
 *------------------------------------------*/
void clif_parse_CloseKafra(int fd, struct map_session_data *sd)
{
	if( sd->state.storage_flag == 1 || sd->state.storage_flag == 3 )
		storage_storageclose(sd);
	else
	if( sd->state.storage_flag == 2 )
		storage_guild_storageclose(sd);
	else return;
	pc_update_last_action(sd,0);
}

/*==========================================
 * Kafra storage protection password system
 *------------------------------------------*/
void clif_parse_StoragePassword(int fd, struct map_session_data *sd)
{
	//TODO
}


/*==========================================
 * Party creation request
 * S 00f9 <party name>.24S
 * S 01e8 <party name>.24S <share flag>.B <share type>.B
 *------------------------------------------*/
void clif_parse_CreateParty(int fd, struct map_session_data *sd)
{
	char* name = (char*)RFIFOP(fd,2);
	name[NAME_LENGTH-1] = '\0';

	if( map[sd->bl.m].flag.partylock )
	{// Party locked.
		clif_displaymessage(fd, msg_txt(227));
		return;
	}
	if( battle_config.basic_skill_check && pc_checkskill(sd,NV_BASIC) < 7 )
	{
		clif_skill_fail(sd,1,0,4);
		return;
	}

	party_create(sd,name,0,0);
}

void clif_parse_CreateParty2(int fd, struct map_session_data *sd)
{
	char* name = (char*)RFIFOP(fd,2);
	int item1 = RFIFOB(fd,26);
	int item2 = RFIFOB(fd,27);
	name[NAME_LENGTH-1] = '\0';

	if( map[sd->bl.m].flag.partylock )
	{// Party locked.
		clif_displaymessage(fd, msg_txt(227));
		return;
	}
	if( battle_config.basic_skill_check && pc_checkskill(sd,NV_BASIC) < 7 )
	{
		clif_skill_fail(sd,1,0,4);
		return;
	}

	party_create(sd,name,item1,item2);
}

/*==========================================
 * Party invitation request
 * S 00fc <account ID>.L
 * S 02c4 <char name>.24S
 *------------------------------------------*/
void clif_parse_PartyInvite(int fd, struct map_session_data *sd)
{
	struct map_session_data *t_sd;
	
	if(map[sd->bl.m].flag.partylock)
	{// Party locked.
		clif_displaymessage(fd, msg_txt(227));
		return;
	}

	t_sd = map_id2sd(RFIFOL(fd,2));

	if(t_sd && t_sd->state.noask)
	{// @noask [LuzZza]
		clif_noask_sub(sd, t_sd, 1);
		return;
	}
	
	party_invite(sd, t_sd);
}

void clif_parse_PartyInvite2(int fd, struct map_session_data *sd)
{
	struct map_session_data *t_sd;
	char *name = (char*)RFIFOP(fd,2);
	name[NAME_LENGTH-1] = '\0';

	if(map[sd->bl.m].flag.partylock)
	{// Party locked.
		clif_displaymessage(fd, msg_txt(227));
		return;
	}

	t_sd = map_nick2sd(name);

	if(t_sd && t_sd->state.noask)
	{// @noask [LuzZza]
		clif_noask_sub(sd, t_sd, 1);
		return;
	}
	
	party_invite(sd, t_sd);
}

/// Party invitation reply (CZ_JOIN_GROUP/CZ_PARTY_JOIN_REQ_ACK)
/// 00ff <party id>.L <flag>.L
/// 02c7 <party id>.L <flag>.B
/// flag:
///     0 = reject
///     1 = accept
void clif_parse_ReplyPartyInvite(int fd,struct map_session_data *sd)
{
	party_reply_invite(sd,RFIFOL(fd,2),RFIFOL(fd,6));
}

void clif_parse_ReplyPartyInvite2(int fd,struct map_session_data *sd)
{
	party_reply_invite(sd,RFIFOL(fd,2),RFIFOB(fd,6));
}

/*==========================================
 * �p�[�e�B�E�ޗv��
 *------------------------------------------*/
void clif_parse_LeaveParty(int fd, struct map_session_data *sd)
{
	if(map[sd->bl.m].flag.partylock)
	{	//Guild locked.
		clif_displaymessage(fd, msg_txt(227));
		return;
	}
	party_leave(sd);
}

/*==========================================
 * �p�[�e�B�����v��
 *------------------------------------------*/
void clif_parse_RemovePartyMember(int fd, struct map_session_data *sd)
{
	if(map[sd->bl.m].flag.partylock)
	{	//Guild locked.
		clif_displaymessage(fd, msg_txt(227));
		return;
	}
	party_removemember(sd,RFIFOL(fd,2),(char*)RFIFOP(fd,6));
}

/*==========================================
 * �p�[�e�B�ݒ�ύX�v��
 *------------------------------------------*/
void clif_parse_PartyChangeOption(int fd, struct map_session_data *sd)
{
	struct party_data *p;
	int i;

	if( !sd->status.party_id )
		return;

	p = party_search(sd->status.party_id);
	if( p == NULL )
		return;

	ARR_FIND( 0, MAX_PARTY, i, p->data[i].sd == sd );
	if( i == MAX_PARTY )
		return; //Shouldn't happen

	if( !p->party.member[i].leader )
		return;

#if PACKETVER < 20090603
	//Client can't change the item-field
	party_changeoption(sd, RFIFOW(fd,2), p->party.item);
#else
	party_changeoption(sd, RFIFOL(fd,2), ((RFIFOB(fd,6)?1:0)+(RFIFOB(fd,7)?2:0)));
#endif
}

/*==========================================
 * Validates and processes party messages
 * S 0108 <packet len>.w <text>.?B (<name> : <message>) 00
 *------------------------------------------*/
void clif_parse_PartyMessage(int fd, struct map_session_data* sd)
{
	const char* text = (char*)RFIFOP(fd,4);
	int textlen = RFIFOW(fd,2) - 4;

	char *name, *message;
	int namelen, messagelen;

	// validate packet and retrieve name and message
	if( !clif_process_message(sd, 0, &name, &namelen, &message, &messagelen) )
		return;

	if( is_atcommand(fd, sd, message, 1)  )
		return;

	if( sd->sc.data[SC_BERSERK] || (sd->sc.data[SC_NOCHAT] && sd->sc.data[SC_NOCHAT]->val1&MANNER_NOCHAT) ||
		(sd->sc.data[SC_DEEPSLEEP] && sd->sc.data[SC_DEEPSLEEP]->val2) || sd->sc.data[SC_SATURDAYNIGHTFEVER])
		return;

	if( battle_config.min_chat_delay )
	{	//[Skotlex]
		if (DIFF_TICK(sd->cantalk_tick, gettick()) > 0)
			return;
		sd->cantalk_tick = gettick() + battle_config.min_chat_delay;
	}

	party_send_message(sd, text, textlen);
}

/*==========================================
 * Changes Party Leader
 * S 07da <account ID>.L
 *------------------------------------------*/
void clif_parse_PartyChangeLeader(int fd, struct map_session_data* sd)
{
	party_changeleader(sd, map_id2sd(RFIFOL(fd,2)));
}

 /*==========================================
 * Party Booking in KRO [Spiria]
 *------------------------------------------*/
void clif_parse_PartyBookingRegisterReq(int fd, struct map_session_data* sd)
{
	short level = RFIFOW(fd,2);
	short mapid = RFIFOW(fd,4);
	short job[PARTY_BOOKING_JOBS];
	int i;
	
	for(i=0; i<PARTY_BOOKING_JOBS; i++)
		job[i] = RFIFOB(fd,6+i*2);

	party_booking_register(sd, level, mapid, job);
}

/*==========================================
 * flag: 0 - success, 1 - failed
 *------------------------------------------*/
void clif_PartyBookingRegisterAck(struct map_session_data *sd, int flag)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x803));
	WFIFOW(fd,0) = 0x803;
	WFIFOW(fd,2) = flag;
	WFIFOSET(fd,packet_len(0x803));
}

void clif_parse_PartyBookingSearchReq(int fd, struct map_session_data* sd)
{	
	short level = RFIFOW(fd,2);
	short mapid = RFIFOW(fd,4);
	short job = RFIFOW(fd,6);
	unsigned long lastindex = RFIFOL(fd,8);
	short resultcount = RFIFOB(fd,12);

	party_booking_search(sd, level, mapid, job, lastindex, resultcount);
}

/*==========================================
 * more_result: 0 - no more, 1 - more
 *------------------------------------------*/
void clif_PartyBookingSearchAck(int fd, struct party_booking_ad_info** results, int count, bool more_result)
{
	int i, j;
	int size = sizeof(struct party_booking_ad_info); // structure size (48)
	struct party_booking_ad_info *pb_ad;
	WFIFOHEAD(fd,size*count + 5);
	WFIFOW(fd,0) = 0x805;
	WFIFOW(fd,2) = size*count + 5;
	WFIFOB(fd,4) = more_result;
	for(i=0; i<count; i++)
	{
		pb_ad = results[i];
		WFIFOL(fd,i*size+5) = pb_ad->index;
		memcpy(WFIFOP(fd,i*size+9),pb_ad->charname,NAME_LENGTH);
		WFIFOL(fd,i*size+33) = pb_ad->starttime;
		WFIFOW(fd,i*size+37) = pb_ad->p_detail.level;
		WFIFOW(fd,i*size+39) = pb_ad->p_detail.mapid;
		for(j=0; j<PARTY_BOOKING_JOBS; j++)
			WFIFOW(fd,i*size+41+j*2) = pb_ad->p_detail.job[j];
	}
	WFIFOSET(fd,WFIFOW(fd,2));
}

void clif_parse_PartyBookingDeleteReq(int fd, struct map_session_data* sd)
{
	if(party_booking_delete(sd))
		clif_PartyBookingDeleteAck(sd, 0);
}

/*==========================================
 * flag: 0 - success, other - failed
 *------------------------------------------*/
void clif_PartyBookingDeleteAck(struct map_session_data* sd, int flag)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x807));
	WFIFOW(fd,0) = 0x807;
	WFIFOW(fd,2) = flag;
	WFIFOSET(fd,packet_len(0x807));
}

void clif_parse_PartyBookingUpdateReq(int fd, struct map_session_data* sd)
{
	short job[PARTY_BOOKING_JOBS];
	int i;
	
	for(i=0; i<PARTY_BOOKING_JOBS; i++)
		job[i] = RFIFOW(fd,2+i*2);

	party_booking_update(sd, job);
}

void clif_PartyBookingInsertNotify(struct map_session_data* sd, struct party_booking_ad_info* pb_ad)
{
	int i;
	uint8 buf[38+PARTY_BOOKING_JOBS*2];

	if(pb_ad == NULL) return;

	WBUFW(buf,0) = 0x809;
	WBUFL(buf,2) = pb_ad->index;
	memcpy(WBUFP(buf,6),pb_ad->charname,NAME_LENGTH);
	WBUFL(buf,30) = pb_ad->starttime;
	WBUFW(buf,34) = pb_ad->p_detail.level;
	WBUFW(buf,36) = pb_ad->p_detail.mapid;
	for(i=0; i<PARTY_BOOKING_JOBS; i++)
		WBUFW(buf,38+i*2) = pb_ad->p_detail.job[i];
	
	clif_send(buf, packet_len(0x809), &sd->bl, ALL_CLIENT);
}

void clif_PartyBookingUpdateNotify(struct map_session_data* sd, struct party_booking_ad_info* pb_ad)
{
	int i;
	uint8 buf[6+PARTY_BOOKING_JOBS*2];

	if(pb_ad == NULL) return;

	WBUFW(buf,0) = 0x80a;
	WBUFL(buf,2) = pb_ad->index;
	for(i=0; i<PARTY_BOOKING_JOBS; i++)
		WBUFW(buf,6+i*2) = pb_ad->p_detail.job[i];
	clif_send(buf,packet_len(0x80a),&sd->bl,ALL_CLIENT); // Now UPDATE all client.
}

void clif_PartyBookingDeleteNotify(struct map_session_data* sd, int index)
{
	uint8 buf[6];
	WBUFW(buf,0) = 0x80b;
	WBUFL(buf,2) = index;
	
	clif_send(buf, packet_len(0x80b), &sd->bl, ALL_CLIENT); // Now UPDATE all client.
}

/*==========================================
 * �I�X��
 *------------------------------------------*/
void clif_parse_CloseVending(int fd, struct map_session_data* sd)
{
	vending_closevending(sd);
}

/*==========================================
 * �I�X�A�C�e�����X�g�v��
 *------------------------------------------*/
void clif_parse_VendingListReq(int fd, struct map_session_data* sd)
{
	if( sd->npc_id )
	{// using an NPC
		return;
	}
	vending_vendinglistreq(sd,RFIFOL(fd,2));
}

/*==========================================
 * Shop item(s) purchase request (CZ_PC_PURCHASE_ITEMLIST_FROMMC)
 * S 0134 <len>.w <ID>.l {<amount>.w <index>.w}.4B*
 *------------------------------------------*/
void clif_parse_PurchaseReq(int fd, struct map_session_data* sd)
{
	int len = (int)RFIFOW(fd,2) - 8;
	int id = (int)RFIFOL(fd,4);
	const uint8* data = (uint8*)RFIFOP(fd,8);

	vending_purchasereq(sd, id, sd->vended_id, data, len/4);

	// whether it fails or not, the buy window is closed
	sd->vended_id = 0;
}

/*==========================================
 * Shop item(s) purchase request (CZ_PC_PURCHASE_ITEMLIST_FROMMC2)
 * S 0801 <len>.w <AID>.l <UniqueID>.l {<amount>.w <index>.w}.4B*
 *------------------------------------------*/
void clif_parse_PurchaseReq2(int fd, struct map_session_data* sd)
{
	int len = (int)RFIFOW(fd,2) - 12;
	int aid = (int)RFIFOL(fd,4);
	int uid = (int)RFIFOL(fd,8);
	const uint8* data = (uint8*)RFIFOP(fd,12);

	vending_purchasereq(sd, aid, uid, data, len/4);

	// whether it fails or not, the buy window is closed
	sd->vended_id = 0;
}

/*==========================================
 * Confirm or cancel the shop preparation window
 * S 01b2 <len>.w <message>.80B <flag>.B {<index>.w <amount>.w <value>.l}.8B*
 * flag: 0=cancel, 1=confirm
 *------------------------------------------*/
void clif_parse_OpenVending(int fd, struct map_session_data* sd)
{
	short len = (short)RFIFOW(fd,2) - 85;
	const char* message = (char*)RFIFOP(fd,4);
	bool flag = (bool)RFIFOB(fd,84);
	const uint8* data = (uint8*)RFIFOP(fd,85);
	char out_msg[MESSAGE_SIZE];

	if( sd->sc.data[SC_NOCHAT] && sd->sc.data[SC_NOCHAT]->val1&MANNER_NOROOM )
		return;

	if( map[sd->bl.m].flag.novending )
	{
		clif_displaymessage (sd->fd, msg_txt(276)); // "You can't open shop on this map"
		return;
	}
	if( battle_config.super_woe_enable )
	{
		clif_displaymessage (sd->fd, "Vending not available on Super WoE / GvG Events.");
		return;
	}
	if( sd->state.secure_items )
	{
		clif_displaymessage(sd->fd, "You can't open vending. Blocked with @security");
		return;
	}

	if( map[sd->bl.m].flag.vending_cell != map_getcell(sd->bl.m,sd->bl.x,sd->bl.y,CELL_CHKNOBOARDS) )
	{
		clif_displaymessage(sd->fd, "You can't open shop on this cell");
		return;
	}
	if( message[0] == '\0' ) // invalid input
		return;

	if( battle_config.vending_zeny_id ) // Extended Vending System
		snprintf(out_msg,MESSAGE_SIZE,"[%s] %s",itemdb_jname(sd->vend_coin),message);
	else
		safestrncpy(out_msg,message,MESSAGE_SIZE);

	vending_openvending(sd, out_msg, flag, data, len/8);
}

int clif_vend(struct map_session_data *sd, int skill_lv)
{
	int c, i;
	int fd;

	nullpo_ret(sd);
	fd = sd->fd;
	c = 0;

	WFIFOHEAD(fd,MAX_COIN_DB*2+4);
	WFIFOW(fd,0) = 0x1ad;

	// Zeny Reserved ItemID
	WFIFOW(fd,c*2+4) = battle_config.vending_zeny_id;
	c++;

	if( battle_config.vending_cash_id )
 	{
		WFIFOW(fd,c*2+4) = battle_config.vending_cash_id;
		c++;
	}

	i = 0;
	while( i < MAX_COIN_DB && coins_db[i] > 0 )
	{
		WFIFOW(fd,c*2+4) = coins_db[i];
		c++;
		i++;
 	}
	sd->menuskill_id = MC_VENDING;
	sd->menuskill_val = skill_lv;
	WFIFOW(fd,2) = c*2+4;
	WFIFOSET(fd,WFIFOW(fd,2));

	return 1;
 }

/*==========================================
 * Guild creation request
 * S 0165 <account id>.L <guild name>.24S
 *------------------------------------------*/
void clif_parse_CreateGuild(int fd,struct map_session_data *sd)
{
	char* name = (char*)RFIFOP(fd,6);
	name[NAME_LENGTH-1] = '\0';

	if(map[sd->bl.m].flag.guildlock)
	{	//Guild locked.
		clif_displaymessage(fd, msg_txt(228));
		return;
	}

	guild_create(sd, name);
}

/*==========================================
 * �M���h�}�X�^�[���ǂ����m�F
 *------------------------------------------*/
void clif_parse_GuildCheckMaster(int fd, struct map_session_data *sd)
{
	clif_guild_masterormember(sd);
}

/*==========================================
 * �M���h���v��
 *------------------------------------------*/
void clif_parse_GuildRequestInfo(int fd, struct map_session_data *sd)
{
	if( !sd->status.guild_id && !sd->bg_id )
		return;

	switch( RFIFOL(fd,2) )
	{
	case 0:	// �M���h��{���A�����G�Ώ��
		clif_guild_basicinfo(sd);
		clif_guild_allianceinfo(sd);
		break;
	case 1:	// �����o�[���X�g�A��E�����X�g
		clif_guild_positionnamelist(sd);
		clif_guild_memberlist(sd);
		break;
	case 2:	// ��E�����X�g�A��E��񃊃X�g
		clif_guild_positionnamelist(sd);
		clif_guild_positioninfolist(sd);
		break;
	case 3:	// �X�L�����X�g
		clif_guild_skillinfo(sd);
		break;
	case 4:	// �Ǖ����X�g
		clif_guild_expulsionlist(sd);
		break;
	default:
		ShowError("clif: guild request info: unknown type %d\n", RFIFOL(fd,2));
		break;
	}
}

/*==========================================
 * �M���h��E�ύX
 *------------------------------------------*/
void clif_parse_GuildChangePositionInfo(int fd, struct map_session_data *sd)
{
	int i;

	if(!sd->state.gmaster_flag)
		return;

	for(i = 4; i < RFIFOW(fd,2); i += 40 ){
		guild_change_position(sd->status.guild_id, RFIFOL(fd,i), RFIFOL(fd,i+4), RFIFOL(fd,i+12), (char*)RFIFOP(fd,i+16));
	}
}

/*==========================================
 * �M���h�����o��E�ύX
 * S 0155 <packet len>.W {<account id>.L <char id>.L <idx>.L}
 *------------------------------------------*/
void clif_parse_GuildChangeMemberPosition(int fd, struct map_session_data *sd)
{
	int i;
	
	if(!sd->state.gmaster_flag)
		return;

	for(i=4;i<RFIFOW(fd,2);i+=12){
		guild_change_memberposition(sd->status.guild_id,
			RFIFOL(fd,i),RFIFOL(fd,i+4),RFIFOL(fd,i+8));
	}
}

/*==========================================
 * �M���h�G���u�����v��
 *------------------------------------------*/
void clif_parse_GuildRequestEmblem(int fd,struct map_session_data *sd)
{
	struct guild* g;
	int guild_id = RFIFOL(fd,2), i;

	if( (g = guild_search(guild_id)) != NULL )
		clif_guild_emblem(sd,g);
	else if( guild_id > INT16_MAX - 13 && guild_id <= INT16_MAX )
	{
		i = (int)(INT16_MAX - guild_id);
		clif_bg_emblem(sd, &bg_guild[i]);
	}
}


/// Validates data of a guild emblem (compressed bitmap)
static bool clif_validate_emblem(const uint8* emblem, unsigned long emblem_len)
{
	bool success;
	uint8 buf[1800];  // no well-formed emblem bitmap is larger than 1782 (24 bit) / 1654 (8 bit) bytes
	unsigned long buf_len = sizeof(buf);

	success = ( decode_zip(buf, &buf_len, emblem, emblem_len) == 0 && buf_len >= 18 )  // sizeof(BITMAPFILEHEADER) + sizeof(biSize) of the following info header struct
			&& RBUFW(buf,0) == 0x4d42   // BITMAPFILEHEADER.bfType (signature)
			&& RBUFL(buf,2) == buf_len  // BITMAPFILEHEADER.bfSize (file size)
			&& RBUFL(buf,10) < buf_len  // BITMAPFILEHEADER.bfOffBits (offset to bitmap bits)
			;

	return success;
}


/*==========================================
 * �M���h�G���u�����ύX
 * S 0153 <packet len>.W <emblem data>.?B
 *------------------------------------------*/
void clif_parse_GuildChangeEmblem(int fd,struct map_session_data *sd)
{
	unsigned long emblem_len = RFIFOW(fd,2)-4;
	const uint8* emblem = RFIFOP(fd,4);

	if( !emblem_len || !sd->state.gmaster_flag )
		return;

	if( !clif_validate_emblem(emblem, emblem_len) )
	{
		ShowWarning("clif_parse_GuildChangeEmblem: Rejected malformed guild emblem (size=%lu, accound_id=%d, char_id=%d, guild_id=%d).\n", emblem_len, sd->status.account_id, sd->status.char_id, sd->status.guild_id);
		return;
	}

	guild_change_emblem(sd, emblem_len, (const char*)emblem);
}

/*==========================================
 * Guild notice update request
 * S 016E <guildID>.l <msg1>.60B <msg2>.120B
 *------------------------------------------*/
void clif_parse_GuildChangeNotice(int fd, struct map_session_data* sd)
{
	int guild_id = RFIFOL(fd,2);
	char* msg1 = (char*)RFIFOP(fd,6);
	char* msg2 = (char*)RFIFOP(fd,66);

	if(!sd->state.gmaster_flag)
		return;

	// compensate for some client defects when using multilanguage mode
	if (msg1[0] == '|' && msg1[3] == '|') msg1+= 3; // skip duplicate marker
	if (msg2[0] == '|' && msg2[3] == '|') msg2+= 3; // skip duplicate marker
	if (msg2[0] == '|') msg2[strnlen(msg2, MAX_GUILDMES2)-1] = '\0'; // delete extra space at the end of string

	guild_change_notice(sd, guild_id, msg1, msg2);
}

/*==========================================
 * �M���h���U
 *------------------------------------------*/
void clif_parse_GuildInvite(int fd,struct map_session_data *sd)
{
	struct map_session_data *t_sd;
	
	if(map[sd->bl.m].flag.guildlock)
	{	//Guild locked.
		clif_displaymessage(fd, msg_txt(228));
		return;
	}

	t_sd = map_id2sd(RFIFOL(fd,2));

	// @noask [LuzZza]
	if(t_sd && t_sd->state.noask) {
		clif_noask_sub(sd, t_sd, 2);
		return;
	}

	guild_invite(sd,t_sd);
}

/*==========================================
 * �M���h���U�ԐM
 *------------------------------------------*/
void clif_parse_GuildReplyInvite(int fd,struct map_session_data *sd)
{
	guild_reply_invite(sd,RFIFOL(fd,2),RFIFOB(fd,6));
}

/*==========================================
 * �M���h�E��
 *------------------------------------------*/
void clif_parse_GuildLeave(int fd,struct map_session_data *sd)
{
	if(map[sd->bl.m].flag.guildlock)
	{	//Guild locked.
		clif_displaymessage(fd, msg_txt(228));
		return;
	}

	if( battle_config.super_woe_enable )
	{
		clif_displaymessage(fd, "You can't leave a Guild in Champions Arena.");
		return;
	}

	if( !guild_canescape(sd) )
	{	//Guild locked.
		clif_displaymessage(fd, "You can't leave until 10 seconds of No War Actions.");
		return;
	}

	if( sd->bg_id )
	{
		clif_displaymessage(fd, "You can't leave battleground guilds.");
		return;
	}

	guild_leave(sd,RFIFOL(fd,2),RFIFOL(fd,6),RFIFOL(fd,10),(char*)RFIFOP(fd,14));
}

/*==========================================
 * Request to expel a member of a guild
 * S 015b <guild_id>.L <account_id>.L <char_id>.L <reason>.40B
 *------------------------------------------*/
void clif_parse_GuildExpulsion(int fd,struct map_session_data *sd)
{
	if( map[sd->bl.m].flag.guildlock || sd->bg_id )
	{ // Guild locked.
		clif_displaymessage(fd, msg_txt(228));
		return;
	}

	if( battle_config.super_woe_enable )
	{
		clif_displaymessage(fd, "You can't expel a member from your guild. Use Guild Control on your guild Flag.");
		return;
	}

	guild_expulsion(sd,RFIFOL(fd,2),RFIFOL(fd,6),RFIFOL(fd,10),(char*)RFIFOP(fd,14));
}

/*==========================================
 * Validates and processes guild messages
 * S 017e <packet len>.w <text>.?B (<name> : <message>) 00
 *------------------------------------------*/
void clif_parse_GuildMessage(int fd, struct map_session_data* sd)
{
	const char* text = (char*)RFIFOP(fd,4);
	int textlen = RFIFOW(fd,2) - 4;

	char *name, *message;
	int namelen, messagelen;

	// validate packet and retrieve name and message
	if( !clif_process_message(sd, 0, &name, &namelen, &message, &messagelen) )
		return;

	if( is_atcommand(fd, sd, message, 1) )
		return;

	if( sd->sc.data[SC_BERSERK] || (sd->sc.data[SC_NOCHAT] && sd->sc.data[SC_NOCHAT]->val1&MANNER_NOCHAT) ||
		(sd->sc.data[SC_DEEPSLEEP] && sd->sc.data[SC_DEEPSLEEP]->val2) || sd->sc.data[SC_SATURDAYNIGHTFEVER])
		return;

	if( battle_config.min_chat_delay )
	{	//[Skotlex]
		if (DIFF_TICK(sd->cantalk_tick, gettick()) > 0)
			return;
		sd->cantalk_tick = gettick() + battle_config.min_chat_delay;
	}

	if( battle_config.bg_eAmod_mode && sd->bg_id )
		bg_send_message(sd, text, textlen);
	else
		guild_send_message(sd, text, textlen);
}

/*==========================================
 * �M���h�����v��
 *------------------------------------------*/
void clif_parse_GuildRequestAlliance(int fd, struct map_session_data *sd)
{
	struct map_session_data *t_sd;
	
	if(!sd->state.gmaster_flag)
		return;

	if(map[sd->bl.m].flag.guildlock)
	{	//Guild locked.
		clif_displaymessage(fd, msg_txt(228));
		return;
	}

	t_sd = map_id2sd(RFIFOL(fd,2));

	// @noask [LuzZza]
	if(t_sd && t_sd->state.noask) {
		clif_noask_sub(sd, t_sd, 3);
		return;
	}
	
	guild_reqalliance(sd,t_sd);
}

/*==========================================
 * �M���h�����v���ԐM
 *------------------------------------------*/
void clif_parse_GuildReplyAlliance(int fd, struct map_session_data *sd)
{
	guild_reply_reqalliance(sd,RFIFOL(fd,2),RFIFOL(fd,6));
}

/*==========================================
 * �M���h�֌W����
 *------------------------------------------*/
void clif_parse_GuildDelAlliance(int fd, struct map_session_data *sd)
{
	if(!sd->state.gmaster_flag)
		return;

	if(map[sd->bl.m].flag.guildlock)
	{	//Guild locked.
		clif_displaymessage(fd, msg_txt(228));
		return;
	}
	guild_delalliance(sd,RFIFOL(fd,2),RFIFOL(fd,6));
}

/*==========================================
 * �M���h�G��
 *------------------------------------------*/
void clif_parse_GuildOpposition(int fd, struct map_session_data *sd)
{
	struct map_session_data *t_sd;

	if(!sd->state.gmaster_flag)
		return;

	if(map[sd->bl.m].flag.guildlock)
	{	//Guild locked.
		clif_displaymessage(fd, msg_txt(228));
		return;
	}

	t_sd = map_id2sd(RFIFOL(fd,2));

	// @noask [LuzZza]
	if(t_sd && t_sd->state.noask) {
		clif_noask_sub(sd, t_sd, 4);
		return;
	}
	
	guild_opposition(sd,t_sd);
}

/*==========================================
 * �M���h���U
 *------------------------------------------*/
void clif_parse_GuildBreak(int fd, struct map_session_data *sd)
{
	if( map[sd->bl.m].flag.guildlock )
	{	//Guild locked.
		clif_displaymessage(fd, msg_txt(228));
		return;
	}
	guild_break(sd,(char*)RFIFOP(fd,2));
}

// pet
void clif_parse_PetMenu(int fd, struct map_session_data *sd)
{
	pet_menu(sd,RFIFOB(fd,2));
}

void clif_parse_CatchPet(int fd, struct map_session_data *sd)
{
	pet_catch_process2(sd,RFIFOL(fd,2));
}

void clif_parse_SelectEgg(int fd, struct map_session_data *sd)
{
	if (sd->menuskill_id != SA_TAMINGMONSTER || sd->menuskill_val != -1)
	{
		//Forged packet, disconnect them [Kevin]
		clif_authfail_fd(fd, 0);
		return;
	}
	pet_select_egg(sd,RFIFOW(fd,2)-2);
	sd->menuskill_val = sd->menuskill_id = 0;
}

void clif_parse_SendEmotion(int fd, struct map_session_data *sd)
{
	if(sd->pd)
		clif_pet_emotion(sd->pd,RFIFOL(fd,2));
}

void clif_parse_ChangePetName(int fd, struct map_session_data *sd)
{
	pet_change_name(sd,(char*)RFIFOP(fd,2));
}

/*==========================================
 * /kill <???>
 * (or right click menu for GM "(name) force to quit")
 * S 00cc <id>.L
 *------------------------------------------*/
void clif_parse_GMKick(int fd, struct map_session_data *sd)
{
	struct block_list *target;
	int tid,lv;

	if( battle_config.atc_gmonly && !pc_isGM(sd) )
		return;

	tid = RFIFOL(fd,2);
	target = map_id2bl(tid);
	if (!target) {
		clif_GM_kickack(sd, 0);
		return;
	}

	switch (target->type) {
	case BL_PC:
	{
		struct map_session_data *tsd = (struct map_session_data *)target;
		if (pc_isGM(sd) <= pc_isGM(tsd))
		{
			clif_GM_kickack(sd, 0);
			return;
		}

		lv = get_atcommand_level(atcommand_kick);
		if( pc_isGM(sd) < lv )
		{
			clif_GM_kickack(sd, 0);
			return;
		}

		if(log_config.gm && lv >= log_config.gm) {
			char message[256];
			sprintf(message, "/kick %s (%d)", tsd->status.name, tsd->status.char_id);
			log_atcommand(sd, message);
		}

		clif_GM_kick(sd, tsd);
	}
	break;
	case BL_MOB:
	{
		lv = get_atcommand_level(atcommand_killmonster);
		if( pc_isGM(sd) < lv )
		{
			clif_GM_kickack(sd, 0);
			return;
		}

		if(log_config.gm && lv >= log_config.gm) {
			char message[256];
			sprintf(message, "/kick %s (%d)", status_get_name(target), status_get_class(target));
			log_atcommand(sd, message);
		}

		status_percent_damage(&sd->bl, target, 100, 0, true); // can invalidate 'target'
	}
	break;
	case BL_NPC:
	{
		struct npc_data* nd = (struct npc_data *)target;
		lv = get_atcommand_level(atcommand_unloadnpc);
		if( pc_isGM(sd) < lv )
		{
			clif_GM_kickack(sd, 0);
			return;
		}

		if( log_config.gm && lv >= log_config.gm ) {
			char message[256];
			sprintf(message, "/kick %s (%d)", status_get_name(target), status_get_class(target));
			log_atcommand(sd, message);
		}

		// copy-pasted from atcommand_unloadnpc
		npc_unload_duplicates(nd);
		npc_unload(nd); // invalidates 'target'
		npc_read_event_script();
	}
	break;
	default:
		clif_GM_kickack(sd, 0);
	}
}

/*==========================================
 * /killall
 *------------------------------------------*/
void clif_parse_GMKickAll(int fd, struct map_session_data* sd)
{
	is_atcommand(fd, sd, "@kickall", 1);
}

/*==========================================
 * /remove <account name>
 * /shift <char name>
 *------------------------------------------*/
void clif_parse_GMShift(int fd, struct map_session_data *sd)
{// FIXME: remove is supposed to receive account name for clients prior 20100803RE
	char *player_name;
	int lv;

	if( battle_config.atc_gmonly && !pc_isGM(sd) )
		return;
	if( pc_isGM(sd) < (lv=get_atcommand_level(atcommand_jumpto)) )
		return;

	player_name = (char*)RFIFOP(fd,2);
	player_name[NAME_LENGTH-1] = '\0';
	atcommand_jumpto(fd, sd, "@jumpto", player_name); // as @jumpto
	if( log_config.gm && lv >= log_config.gm ) {
		char message[NAME_LENGTH+7];
		sprintf(message, "/shift %s", player_name);
		log_atcommand(sd, message);
	}
}


/// Warps oneself to the location of the character given by account id ( /remove <account id> ).
/// R 0843 <account id>.L
void clif_parse_GMRemove2(int fd, struct map_session_data* sd)
{
	int account_id, lv;
	struct map_session_data* pl_sd;

	if( battle_config.atc_gmonly && !pc_isGM(sd) )
	{
		return;
	}

	if( pc_isGM(sd) < ( lv = get_atcommand_level(atcommand_jumpto) ) )
	{
		return;
	}

	account_id = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);

	if( ( pl_sd = map_id2sd(account_id) ) != NULL && pc_isGM(sd) >= pc_isGM(pl_sd) )
	{
		pc_warpto(sd, pl_sd);
	}

	if( log_config.gm && lv >= log_config.gm )
	{
		char message[32];

		sprintf(message, "/remove %d", account_id);
		log_atcommand(sd, message);
	}
}


/*==========================================
 * /recall <account name>
 * /summon <char name>
 *------------------------------------------*/
void clif_parse_GMRecall(int fd, struct map_session_data *sd)
{// FIXME: recall is supposed to receive account name for clients prior 20100803RE
	char *player_name;
	int lv;

	if( battle_config.atc_gmonly && !pc_isGM(sd) )
		return;

	if( pc_isGM(sd) < (lv=get_atcommand_level(atcommand_recall)) )
		return;

	player_name = (char*)RFIFOP(fd,2);
	player_name[NAME_LENGTH-1] = '\0';
	atcommand_recall(fd, sd, "@recall", player_name); // as @recall
	if( log_config.gm && lv >= log_config.gm ) {
		char message[NAME_LENGTH+8];
		sprintf(message, "/recall %s", player_name);
		log_atcommand(sd, message);
	}
}


/// Summons a character given by account id to one's own position ( /recall <account id> )
/// R 0842 <account id>.L
void clif_parse_GMRecall2(int fd, struct map_session_data* sd)
{
	int account_id, lv;
	struct map_session_data* pl_sd;

	if( battle_config.atc_gmonly && !pc_isGM(sd) )
	{
		return;
	}

	if( pc_isGM(sd) < ( lv = get_atcommand_level(atcommand_recall) ) )
	{
		return;
	}

	account_id = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);

	if( ( pl_sd = map_id2sd(account_id) ) != NULL && pc_isGM(sd) >= pc_isGM(pl_sd) )
	{
		pc_recall(sd, pl_sd);
	}

	if( log_config.gm && lv >= log_config.gm )
	{
		char message[32];

		sprintf(message, "/recall %d", account_id);
		log_atcommand(sd, message);
	}
}


/*==========================================
 * /monster /item 
 * R 01F3 <name>.24B
 *------------------------------------------*/
void clif_parse_GM_Monster_Item(int fd, struct map_session_data *sd)
{
	char *monster_item_name;
	char message[NAME_LENGTH+10]; //For logging.
	int level;

	if( battle_config.atc_gmonly && !pc_isGM(sd) )
		return;

	monster_item_name = (char*)RFIFOP(fd,2);
	monster_item_name[NAME_LENGTH-1] = '\0';

	if( mobdb_searchname(monster_item_name) ) {
		if( pc_isGM(sd) < (level=get_atcommand_level(atcommand_monster)) )
			return;
		atcommand_monster(fd, sd, "@monster", monster_item_name); // as @monster
		if( log_config.gm && level >= log_config.gm )
		{	//Log action. [Skotlex]
			snprintf(message, sizeof(message)-1, "@spawn %s", monster_item_name);
			log_atcommand(sd, message);
		}
		return;
	}
	if( itemdb_searchname(monster_item_name) == NULL )
		return;
	if( pc_isGM(sd) < (level = get_atcommand_level(atcommand_item)) )
		return;
	atcommand_item(fd, sd, "@item", monster_item_name); // as @item
	if( log_config.gm && level >= log_config.gm )
	{	//Log action. [Skotlex]
		sprintf(message, "@item %s", monster_item_name);
		log_atcommand(sd, message);
	}
}

/*==========================================
 * /hide
 *------------------------------------------*/
void clif_parse_GMHide(int fd, struct map_session_data *sd)
{
	if( battle_config.atc_gmonly && !pc_isGM(sd) )
		return;

	if( pc_isGM(sd) < get_atcommand_level(atcommand_hide) )
		return;

	if( sd->sc.option & OPTION_INVISIBLE ) {
		sd->sc.option &= ~OPTION_INVISIBLE;
		if (sd->disguise)
			status_set_viewdata(&sd->bl, sd->disguise);
		else
			status_set_viewdata(&sd->bl, sd->status.class_);
		clif_displaymessage(fd, "Invisible: Off.");
	} else {
		sd->sc.option |= OPTION_INVISIBLE;
		sd->vd.class_ = INVISIBLE_CLASS;
		clif_displaymessage(fd, "Invisible: On.");
		if( log_config.gm && get_atcommand_level(atcommand_hide) >= log_config.gm )
			log_atcommand(sd, "/hide");
	}
	clif_changeoption(&sd->bl);
}

/*==========================================
 * GM adjustment of a player's manner value (right-click GM menu)
 * S 0149 <id>.L <type>.B <value>.W
 * type: 0 - positive points
 *       1 - negative points
 *       2 - self mute (+10 minutes)
 *------------------------------------------*/
void clif_parse_GMReqNoChat(int fd,struct map_session_data *sd)
{
	int id, type, value, level;
	struct map_session_data *dstsd;

	id = RFIFOL(fd,2);
	type = RFIFOB(fd,6);
	value = RFIFOW(fd,7);

	if( type == 0 )
		value = 0 - value;

	//If type is 2 and the ids don't match, this is a crafted hacked packet!
	//Disabled because clients keep self-muting when you give players public @ commands... [Skotlex]
	if (type == 2 /* && (pc_isGM(sd) > 0 || sd->bl.id != id)*/)
		return;

	dstsd = map_id2sd(id);
	if( dstsd == NULL )
		return;

	if( (level = pc_isGM(sd)) > pc_isGM(dstsd) && level >= get_atcommand_level(atcommand_mute) )
	{
		clif_manner_message(sd, 0);
		clif_manner_message(dstsd, 5);

		if( dstsd->status.manner < value ) {
			dstsd->status.manner -= value;
			sc_start(&dstsd->bl,SC_NOCHAT,100,0,0);
		} else {
			dstsd->status.manner = 0;
			status_change_end(&dstsd->bl, SC_NOCHAT, INVALID_TIMER);
		}

		if( type != 2 )
			clif_GM_silence(sd, dstsd, type);
	}
}

/*==========================================
 * GM adjustment of a player's manner value by -60 (using name)
 * /rc <name>
 * S 0212 <name>.24B
 *------------------------------------------*/
void clif_parse_GMRc(int fd, struct map_session_data* sd)
{
	char* name = (char*)RFIFOP(fd,2);
	struct map_session_data* dstsd;
	name[23] = '\0';
	dstsd = map_nick2sd(name);
	if( dstsd == NULL )
		return;

	if( pc_isGM(sd) > pc_isGM(dstsd) && pc_isGM(sd) >= get_atcommand_level(atcommand_mute) )
	{
		clif_manner_message(sd, 0);
		clif_manner_message(dstsd, 3);

		dstsd->status.manner -= 60;
		sc_start(&dstsd->bl,SC_NOCHAT,100,0,0);

		clif_GM_silence(sd, dstsd, 1);
	}
}

/*==========================================
 * GM requesting account name (for right-click gm menu)
 * S 01df <id>.L
 * R 01e0 <id>.L <name>.24B
 *------------------------------------------*/
void clif_parse_GMReqAccountName(int fd, struct map_session_data *sd)
{
	int tid;
	tid = RFIFOL(fd,2);

	//TODO: find out if this works for any player or only for authorized GMs

	WFIFOHEAD(fd,packet_len(0x1e0));
	WFIFOW(fd,0) = 0x1e0;
	WFIFOL(fd,2) = tid;
	safestrncpy((char*)WFIFOP(fd,6), "", 24); // insert account name here >_<
	WFIFOSET(fd, packet_len(0x1e0));
}

/*==========================================
 * GM single cell type change request
 * /changemaptype <x> <y> <type:0-1>
 * S 0198 <x>.W <y>.W <gat>.W
 *------------------------------------------*/
void clif_parse_GMChangeMapType(int fd, struct map_session_data *sd)
{
	int x,y,type;

	if( battle_config.atc_gmonly && !pc_isGM(sd) )
		return;

	if( pc_isGM(sd) < 99 ) //TODO: add proper check
		return;

	x = RFIFOW(fd,2);
	y = RFIFOW(fd,4);
	type = RFIFOW(fd,6);

	map_setgatcell(sd->bl.m,x,y,type);
	clif_changemapcell(0,sd->bl.m,x,y,type,ALL_SAMEMAP);
	//FIXME: once players leave the map, the client 'forgets' this information.
}

/// S 00cf <nick>.24B <type>.B
/// type: 0 (/ex nick) deny speech from nick
///       1 (/in nick) allow speech from nick
///
/// R 00d1 <type>.B <result>.B
/// type:   0: deny, 1: allow
/// result: 0: success, 1: fail 2: list full
void clif_parse_PMIgnore(int fd, struct map_session_data* sd)
{
	char output[512];
	char* nick;
	uint8 type;
	int i;

	nick = (char*)RFIFOP(fd,2); // speed up
	nick[NAME_LENGTH-1] = '\0'; // to be sure that the player name has at most 23 characters
	type = RFIFOB(fd,26);

	WFIFOHEAD(fd,packet_len(0xd1));
	WFIFOW(fd,0) = 0x0d1;
	WFIFOB(fd,2) = type;
	
	if( type == 0 )
	{	// Add name to ignore list (block)

		// Bot-check...
		if (strcmp(wisp_server_name, nick) == 0)
		{	// to find possible bot users who automaticaly ignore people
			sprintf(output, "Character '%s' (account: %d) has tried to block wisps from '%s' (wisp name of the server). Bot user?", sd->status.name, sd->status.account_id, wisp_server_name);
			intif_wis_message_to_gm(wisp_server_name, battle_config.hack_info_GM_level, output);
			WFIFOB(fd,3) = 1; // fail
			WFIFOSET(fd, packet_len(0x0d1));
			return;
		}

		// try to find a free spot, while checking for duplicates at the same time
		ARR_FIND( 0, MAX_IGNORE_LIST, i, sd->ignore[i].name[0] == '\0' || strcmp(sd->ignore[i].name, nick) == 0 );
		if( i == MAX_IGNORE_LIST )
		{// no space for new entry
			WFIFOB(fd,3) = 2; // fail
			WFIFOSET(fd, packet_len(0x0d1));
			return;
		}
		if( sd->ignore[i].name[0] != '\0' )
		{// name already exists
			WFIFOB(fd,3) = 0; // Aegis reports success.
			WFIFOSET(fd, packet_len(0x0d1));
			return;
		}

		//Insert in position i
		safestrncpy(sd->ignore[i].name, nick, NAME_LENGTH);

		WFIFOB(fd,3) = 0; // success
		WFIFOSET(fd, packet_len(0x0d1));
	}
	else
	{	// Remove name from ignore list (unblock)

		// find entry
		ARR_FIND( 0, MAX_IGNORE_LIST, i, sd->ignore[i].name[0] == '\0' || strcmp(sd->ignore[i].name, nick) == 0 );
		if( i == MAX_IGNORE_LIST || sd->ignore[i].name[i] == '\0' )
		{ //Not found
			WFIFOB(fd,3) = 1; // fail
			WFIFOSET(fd, packet_len(0x0d1));
			return;
		}
		// move everything one place down to overwrite removed entry
		memmove(sd->ignore[i].name, sd->ignore[i+1].name, (MAX_IGNORE_LIST-i-1)*sizeof(sd->ignore[0].name));
		// wipe last entry
		memset(sd->ignore[MAX_IGNORE_LIST-1].name, 0, sizeof(sd->ignore[0].name));

		WFIFOB(fd,3) = 0; // success
		WFIFOSET(fd, packet_len(0x0d1));
	}

	return;
}

/// S 00d0 <type>.B
/// type: 0 (/exall) deny all speech
///       1 (/inall) allow all speech
///
/// R 00d2 <type>.B <fail>.B
/// type: 0: deny, 1: allow
/// fail: 0: success, 1: fail
void clif_parse_PMIgnoreAll(int fd, struct map_session_data *sd)
{
	WFIFOHEAD(fd,packet_len(0xd2));
	WFIFOW(fd,0) = 0x0d2;
	WFIFOB(fd,2) = RFIFOB(fd,2);

	if( RFIFOB(fd,2) == 0 )
	{// Deny all
		if( sd->state.ignoreAll ) {
			WFIFOB(fd,3) = 1; // fail
		} else {
			sd->state.ignoreAll = 1;
			WFIFOB(fd,3) = 0; // success
		}
	}
	else
	{//Unblock everyone
		if( sd->state.ignoreAll ) {
			sd->state.ignoreAll = 0;
			WFIFOB(fd,3) = 0; // success
		} else {
			if (sd->ignore[0].name[0] != '\0')
			{  //Wipe the ignore list.
				memset(sd->ignore, 0, sizeof(sd->ignore));
				WFIFOB(fd,3) = 0; // success
			} else {
				WFIFOB(fd,3) = 1; // fail
			}
		}
	}

	WFIFOSET(fd, packet_len(0x0d2));
	return;
}

/*==========================================
 * Wis���ۃ��X�g
 *------------------------------------------*/
void clif_parse_PMIgnoreList(int fd,struct map_session_data *sd)
{
	int i;

	WFIFOHEAD(fd, 4 + (NAME_LENGTH * MAX_IGNORE_LIST));
	WFIFOW(fd,0) = 0xd4;

	for(i = 0; i < MAX_IGNORE_LIST && sd->ignore[i].name[0] != '\0'; i++)
		memcpy(WFIFOP(fd, 4 + i * NAME_LENGTH),sd->ignore[i].name, NAME_LENGTH);

	WFIFOW(fd,2) = 4 + i * NAME_LENGTH;
	WFIFOSET(fd, WFIFOW(fd,2));
	return;
}

/*==========================================
 * �X�p�m�r��/doridori�ɂ��SPR2�{
 *------------------------------------------*/
void clif_parse_NoviceDoriDori(int fd, struct map_session_data *sd)
{
	if (sd->state.doridori) return;

	switch (sd->class_&MAPID_UPPERMASK)
	{
		case MAPID_SOUL_LINKER:
		case MAPID_STAR_GLADIATOR:
		case MAPID_TAEKWON:
			if (!sd->state.rest)
				break;
		case MAPID_SUPER_NOVICE:
			sd->state.doridori=1;
			break;	
	}
	return;
}


/// Request to invoke the effect of super novice's guardian angel prayer (CZ_CHOPOKGI)
/// 01ed
/// Note: This packet is caused by 7 lines of any text, followed by
///       the prayer and an another line of any text. The prayer is
///       defined by lines 791~794 in data\msgstringtable.txt
///       "Dear angel, can you hear my voice?"
///       "I am" (space separated player name) "Super Novice~"
///       "Help me out~ Please~ T_T"
void clif_parse_NoviceExplosionSpirits(int fd, struct map_session_data *sd)
{
	if( ( sd->class_&MAPID_UPPERMASK ) == MAPID_SUPER_NOVICE )
	{
		unsigned int next = pc_nextbaseexp(sd);

		if( next )
		{
			int percent = (int)( ( (float)sd->status.base_exp/(float)next )*1000. );

			if( percent && ( percent%100 ) == 0 )
			{// 10.0%, 20.0%, ..., 90.0%
				sc_start(&sd->bl, status_skill2sc(MO_EXPLOSIONSPIRITS), 100, 17, skill_get_time(MO_EXPLOSIONSPIRITS, 5)); //Lv17-> +50 critical (noted by Poki) [Skotlex]
				clif_skill_nodamage(&sd->bl, &sd->bl, MO_EXPLOSIONSPIRITS, 5, 1);  // prayer always shows successful Lv5 cast and disregards noskill restrictions
			}
		}
	}
}


/*==========================================
 * Friends List
 *------------------------------------------*/
void clif_friendslist_toggle(struct map_session_data *sd,int account_id, int char_id, int online)
{	//Toggles a single friend online/offline [Skotlex]
	int i, fd = sd->fd;

	//Seek friend.
	for (i = 0; i < MAX_FRIENDS && sd->status.friends[i].char_id &&
		(sd->status.friends[i].char_id != char_id || sd->status.friends[i].account_id != account_id); i++);

	if(i == MAX_FRIENDS || sd->status.friends[i].char_id == 0)
		return; //Not found

	WFIFOHEAD(fd,packet_len(0x206));
	WFIFOW(fd, 0) = 0x206;
	WFIFOL(fd, 2) = sd->status.friends[i].account_id;
	WFIFOL(fd, 6) = sd->status.friends[i].char_id;
	WFIFOB(fd,10) = !online; //Yeah, a 1 here means "logged off", go figure... 
	WFIFOSET(fd, packet_len(0x206));
}

//Subfunction called from clif_foreachclient to toggle friends on/off [Skotlex]
int clif_friendslist_toggle_sub(struct map_session_data *sd,va_list ap)
{
	int account_id, char_id, online;
	account_id = va_arg(ap, int);
	char_id = va_arg(ap, int);
	online = va_arg(ap, int);
	clif_friendslist_toggle(sd, account_id, char_id, online);
	return 0;
}

//For sending the whole friends list.
void clif_friendslist_send(struct map_session_data *sd)
{
	int i = 0, n, fd = sd->fd;
	
	// Send friends list
	WFIFOHEAD(fd, MAX_FRIENDS * 32 + 4);
	WFIFOW(fd, 0) = 0x201;
	for(i = 0; i < MAX_FRIENDS && sd->status.friends[i].char_id; i++)
	{
		WFIFOL(fd, 4 + 32 * i + 0) = sd->status.friends[i].account_id;
		WFIFOL(fd, 4 + 32 * i + 4) = sd->status.friends[i].char_id;
		memcpy(WFIFOP(fd, 4 + 32 * i + 8), &sd->status.friends[i].name, NAME_LENGTH);
	}

	if (i) {
		WFIFOW(fd,2) = 4 + 32 * i;
		WFIFOSET(fd, WFIFOW(fd,2));
	}
	
	for (n = 0; n < i; n++)
	{	//Sending the online players
		if (map_charid2sd(sd->status.friends[n].char_id))
			clif_friendslist_toggle(sd, sd->status.friends[n].account_id, sd->status.friends[n].char_id, 1);
	}
}

/// Reply for add friend request: (success => type 0)
/// type=0 : MsgStringTable[821]="You have become friends with (%s)."
/// type=1 : MsgStringTable[822]="(%s) does not want to be friends with you."
/// type=2 : MsgStringTable[819]="Your Friend List is full."
/// type=3 : MsgStringTable[820]="(%s)'s Friend List is full."
void clif_friendslist_reqack(struct map_session_data *sd, struct map_session_data *f_sd, int type)
{
	int fd;
	nullpo_retv(sd);

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0x209));
	WFIFOW(fd,0) = 0x209;
	WFIFOW(fd,2) = type;
	if (f_sd)
	{
		WFIFOL(fd,4) = f_sd->status.account_id;
		WFIFOL(fd,8) = f_sd->status.char_id;
		memcpy(WFIFOP(fd, 12), f_sd->status.name,NAME_LENGTH);
	}
	WFIFOSET(fd, packet_len(0x209));
}

void clif_parse_FriendsListAdd(int fd, struct map_session_data *sd)
{
	struct map_session_data *f_sd;
	int i, f_fd;

	f_sd = map_nick2sd((char*)RFIFOP(fd,2));

	// Friend doesn't exist (no player with this name)
	if (f_sd == NULL) {
		clif_displaymessage(fd, msg_txt(3));
		return;
	}

	if( sd->bl.id == f_sd->bl.id )
	{// adding oneself as friend
		return;
	}

	// @noask [LuzZza]
	if(f_sd->state.noask) {
		clif_noask_sub(sd, f_sd, 5);
		return;
	}

	// Friend already exists
	for (i = 0; i < MAX_FRIENDS && sd->status.friends[i].char_id != 0; i++) {
		if (sd->status.friends[i].char_id == f_sd->status.char_id) {
			clif_displaymessage(fd, "Friend already exists.");
			return;
		}
	}

	if (i == MAX_FRIENDS) {
		//No space, list full.
		clif_friendslist_reqack(sd, f_sd, 2);
		return;
	}
		
	f_fd = f_sd->fd;
	WFIFOHEAD(f_fd,packet_len(0x207));
	WFIFOW(f_fd,0) = 0x207;
	WFIFOL(f_fd,2) = sd->status.account_id;
	WFIFOL(f_fd,6) = sd->status.char_id;
	memcpy(WFIFOP(f_fd,10), sd->status.name, NAME_LENGTH);
	WFIFOSET(f_fd, packet_len(0x207));

	return;
}

void clif_parse_FriendsListReply(int fd, struct map_session_data *sd)
{
	//<W: id> <L: Player 1 chara ID> <L: Player 1 AID> <B: Response>
	struct map_session_data *f_sd;
	int char_id, account_id;
	char reply;

	account_id = RFIFOL(fd,2);
	char_id = RFIFOL(fd,6);
	reply = RFIFOB(fd,10);

	if( sd->bl.id == account_id )
	{// adding oneself as friend
		return;
	}

	f_sd = map_id2sd(account_id); //The account id is the same as the bl.id of players.
	if (f_sd == NULL)
		return;
		
	if (reply == 0)
		clif_friendslist_reqack(f_sd, sd, 1);
	else {
		int i;
		// Find an empty slot
		for (i = 0; i < MAX_FRIENDS; i++)
			if (f_sd->status.friends[i].char_id == 0)
				break;
		if (i == MAX_FRIENDS) {
			clif_friendslist_reqack(f_sd, sd, 2);
			return;
		}

		f_sd->status.friends[i].account_id = sd->status.account_id;
		f_sd->status.friends[i].char_id = sd->status.char_id;
		memcpy(f_sd->status.friends[i].name, sd->status.name, NAME_LENGTH);
		clif_friendslist_reqack(f_sd, sd, 0);

		if (battle_config.friend_auto_add) {
			// Also add f_sd to sd's friendlist.
			for (i = 0; i < MAX_FRIENDS; i++) {
				if (sd->status.friends[i].char_id == f_sd->status.char_id)
					return; //No need to add anything.
				if (sd->status.friends[i].char_id == 0)
					break;
			}
			if (i == MAX_FRIENDS) {
				clif_friendslist_reqack(sd, f_sd, 2);
				return;
			}

			sd->status.friends[i].account_id = f_sd->status.account_id;
			sd->status.friends[i].char_id = f_sd->status.char_id;
			memcpy(sd->status.friends[i].name, f_sd->status.name, NAME_LENGTH);
			clif_friendslist_reqack(sd, f_sd, 0);
		}
	}

	return;
}

void clif_parse_FriendsListRemove(int fd, struct map_session_data *sd)
{
	// 0x203 </o> <ID to be removed W 4B>
	int account_id, char_id;
	int i, j;

	account_id = RFIFOL(fd,2);
	char_id = RFIFOL(fd,6);

	// Search friend
	for (i = 0; i < MAX_FRIENDS &&
		(sd->status.friends[i].char_id != char_id || sd->status.friends[i].account_id != account_id); i++);

	if (i == MAX_FRIENDS) {
		clif_displaymessage(fd, "Name not found in list.");
		return;
	}
		
	// move all chars down
	for(j = i + 1; j < MAX_FRIENDS; j++)
		memcpy(&sd->status.friends[j-1], &sd->status.friends[j], sizeof(sd->status.friends[0]));

	memset(&sd->status.friends[MAX_FRIENDS-1], 0, sizeof(sd->status.friends[MAX_FRIENDS-1]));
	clif_displaymessage(fd, "Friend removed");
	
	WFIFOHEAD(fd,packet_len(0x20a));
	WFIFOW(fd,0) = 0x20a;
	WFIFOL(fd,2) = account_id;
	WFIFOL(fd,6) = char_id;
	WFIFOSET(fd, packet_len(0x20a));
//	clif_friendslist_send(sd); //This is not needed anymore.

	return;
}

/*==========================================
 * /pvpinfo (CZ_REQ_PVPPOINT & ZC_ACK_PVPPOINT)
 * R 020f <char id>.L <account id>.L
 * S 0210 <char id>.L <account id>.L <win point>.L <lose point>.L <point>.L
 *------------------------------------------*/
void clif_parse_PVPInfo(int fd,struct map_session_data *sd)
{
	WFIFOHEAD(fd,packet_len(0x210));
	WFIFOW(fd,0) = 0x210;
	WFIFOL(fd,2) = sd->status.char_id;
	WFIFOL(fd,6) = sd->status.account_id;
	WFIFOL(fd,10) = sd->pvp_won;	// times won
	WFIFOL(fd,14) = sd->pvp_lost;	// times lost
	WFIFOL(fd,18) = sd->pvp_point;
	WFIFOSET(fd, packet_len(0x210));
}

/*==========================================
 * /blacksmith
 * S 0217
 *------------------------------------------*/
void clif_parse_Blacksmith(int fd,struct map_session_data *sd)
{
	int i;
	const char* name;

	WFIFOHEAD(fd,packet_len(0x219));
	WFIFOW(fd,0) = 0x219;
	//Packet size limits this list to 10 elements. [Skotlex]
	for (i = 0; i < 10 && i < MAX_FAME_LIST; i++) {
		if (smith_fame_list[i].id > 0) {
			if (strcmp(smith_fame_list[i].name, "-") == 0 &&
				(name = map_charid2nick(smith_fame_list[i].id)) != NULL)
			{
				strncpy((char *)(WFIFOP(fd, 2 + 24 * i)), name, NAME_LENGTH);
			} else
				strncpy((char *)(WFIFOP(fd, 2 + 24 * i)), smith_fame_list[i].name, NAME_LENGTH);
		} else
			strncpy((char *)(WFIFOP(fd, 2 + 24 * i)), "None", 5);
		WFIFOL(fd, 242 + i * 4) = smith_fame_list[i].fame;
	}
	for(;i < 10; i++) { //In case the MAX is less than 10.
		strncpy((char *)(WFIFOP(fd, 2 + 24 * i)), "Unavailable", 12);
		WFIFOL(fd, 242 + i * 4) = 0;
	}

	WFIFOSET(fd, packet_len(0x219));
}

int clif_fame_blacksmith(struct map_session_data *sd, int points)
{
	int fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0x21b));
	WFIFOW(fd,0) = 0x21b;
	WFIFOL(fd,2) = points;
	WFIFOL(fd,6) = sd->status.fame;
	WFIFOSET(fd, packet_len(0x21b));

	return 0;
}

/*==========================================
 * /alchemist
 * S 0218
 *------------------------------------------*/
void clif_parse_Alchemist(int fd,struct map_session_data *sd)
{
	int i;
	const char* name;

	WFIFOHEAD(fd,packet_len(0x21a));
	WFIFOW(fd,0) = 0x21a;
	//Packet size limits this list to 10 elements. [Skotlex]
	for (i = 0; i < 10 && i < MAX_FAME_LIST; i++) {
		if (chemist_fame_list[i].id > 0) {
			if (strcmp(chemist_fame_list[i].name, "-") == 0 &&
				(name = map_charid2nick(chemist_fame_list[i].id)) != NULL)
			{
				memcpy(WFIFOP(fd, 2 + 24 * i), name, NAME_LENGTH);
			} else
				memcpy(WFIFOP(fd, 2 + 24 * i), chemist_fame_list[i].name, NAME_LENGTH);
		} else
			memcpy(WFIFOP(fd, 2 + 24 * i), "None", NAME_LENGTH);
		WFIFOL(fd, 242 + i * 4) = chemist_fame_list[i].fame;
	}
	for(;i < 10; i++) { //In case the MAX is less than 10.
		memcpy(WFIFOP(fd, 2 + 24 * i), "Unavailable", NAME_LENGTH);
		WFIFOL(fd, 242 + i * 4) = 0;
	}

	WFIFOSET(fd, packet_len(0x21a));
}

int clif_fame_alchemist(struct map_session_data *sd, int points)
{
	int fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0x21c));
	WFIFOW(fd,0) = 0x21c;
	WFIFOL(fd,2) = points;
	WFIFOL(fd,6) = sd->status.fame;
	WFIFOSET(fd, packet_len(0x21c));
	
	return 0;
}

/*==========================================
 * /taekwon
 * S 0225
 *------------------------------------------*/
void clif_parse_Taekwon(int fd,struct map_session_data *sd)
{
	int i;
	const char* name;

	WFIFOHEAD(fd,packet_len(0x226));
	WFIFOW(fd,0) = 0x226;
	//Packet size limits this list to 10 elements. [Skotlex]
	for (i = 0; i < 10 && i < MAX_FAME_LIST; i++) {
		if (taekwon_fame_list[i].id > 0) {
			if (strcmp(taekwon_fame_list[i].name, "-") == 0 &&
				(name = map_charid2nick(taekwon_fame_list[i].id)) != NULL)
			{
				memcpy(WFIFOP(fd, 2 + 24 * i), name, NAME_LENGTH);
			} else
				memcpy(WFIFOP(fd, 2 + 24 * i), taekwon_fame_list[i].name, NAME_LENGTH);
		} else
			memcpy(WFIFOP(fd, 2 + 24 * i), "None", NAME_LENGTH);
		WFIFOL(fd, 242 + i * 4) = taekwon_fame_list[i].fame;
	}
	for(;i < 10; i++) { //In case the MAX is less than 10.
		memcpy(WFIFOP(fd, 2 + 24 * i), "Unavailable", NAME_LENGTH);
		WFIFOL(fd, 242 + i * 4) = 0;
	}
	WFIFOSET(fd, packet_len(0x226));
}

int clif_fame_taekwon(struct map_session_data *sd, int points)
{
	int fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0x224));
	WFIFOW(fd,0) = 0x224;
	WFIFOL(fd,2) = points;
	WFIFOL(fd,6) = sd->status.fame;
	WFIFOSET(fd, packet_len(0x224));
	
	return 0;
}

/*==========================================
 * /pk
 * S 0237
 *------------------------------------------*/
void clif_parse_RankingPk(int fd,struct map_session_data *sd)
{
	int i;
	const char* name;

	WFIFOHEAD(fd,packet_len(0x238));
	WFIFOW(fd,0) = 0x238;
	for(i = 0;i < 10 && i < MAX_FAME_LIST; i++) {
		if (pvprank_fame_list[i].id > 0) {
			if (strcmp(pvprank_fame_list[i].name, "-") == 0 &&
				(name = map_charid2nick(pvprank_fame_list[i].id)) != NULL)
			{
				memcpy(WFIFOP(fd, i * 24 + 2), name, NAME_LENGTH);
			} else
				memcpy(WFIFOP(fd, i * 24 + 2), pvprank_fame_list[i].name, NAME_LENGTH);
		} else
			memcpy(WFIFOP(fd,i*24+2), "None", NAME_LENGTH);
		WFIFOL(fd,i * 4 + 242) = pvprank_fame_list[i].fame;
	}
	for(;i < 10; i++) { //In case the MAX is less than 10.
		memcpy(WFIFOP(fd, i * 24 + 2), "Unavailable", NAME_LENGTH);
		WFIFOL(fd, i * 4 + 242) = 0;
	}
	WFIFOSET(fd, packet_len(0x238));
	return;
}

int clif_rank_info(struct map_session_data *sd, int points, int total, int flag)
{
	char message[100];
	if( points <= 0 )
		return 0;

	if( !flag )
	{
		sprintf(message, "[Your Player Killer Rank +%d = %d points]", points, total);
		clif_specialeffect(&sd->bl,9,AREA);
	}
	else
		sprintf(message, "[Your Battleground Rank +%d = %d points]", points, total);

	clif_displaymessage(sd->fd, message);
	return 0;
}

/*==========================================
 * SG Feel save OK [Komurka]
 *------------------------------------------*/
void clif_parse_FeelSaveOk(int fd,struct map_session_data *sd)
{
	int i;
	if (sd->menuskill_id != SG_FEEL)
		return;
	i = sd->menuskill_val-1;
	if (i<0 || i >= MAX_PC_FEELHATE) return; //Bug?

	sd->feel_map[i].index = map_id2index(sd->bl.m);
	sd->feel_map[i].m = sd->bl.m;
	pc_setglobalreg(sd,sg_info[i].feel_var,sd->feel_map[i].index);

//Are these really needed? Shouldn't they show up automatically from the feel save packet?
//	clif_misceffect2(&sd->bl, 0x1b0);
//	clif_misceffect2(&sd->bl, 0x21f);
	clif_feel_info(sd, i, 0);
	sd->menuskill_val = sd->menuskill_id = 0;
}

/// Star Gladiator's Feeling map confirmation prompt (ZC_STARPLACE)
void clif_feel_req(int fd, struct map_session_data *sd, int skilllv)
{
	WFIFOHEAD(fd,packet_len(0x253));
	WFIFOW(fd,0)=0x253;
	WFIFOSET(fd, packet_len(0x253));
	sd->menuskill_id = SG_FEEL;
	sd->menuskill_val = skilllv;
}

/*==========================================
 * Homunculus packets
 *------------------------------------------*/
void clif_parse_ChangeHomunculusName(int fd, struct map_session_data *sd)
{
	merc_hom_change_name(sd,(char*)RFIFOP(fd,2));
}

void clif_parse_HomMoveToMaster(int fd, struct map_session_data *sd)
{
	int id = RFIFOL(fd,2); // Mercenary or Homunculus
	struct block_list *bl = NULL;
	struct unit_data *ud = NULL;

	if( sd->md && sd->md->bl.id == id )
		bl = &sd->md->bl;
	else if( merc_is_hom_active(sd->hd) && sd->hd->bl.id == id )
		bl = &sd->hd->bl; // Moving Homunculus
	else return;

	unit_calc_pos(bl, sd->bl.x, sd->bl.y, sd->ud.dir);
	ud = unit_bl2ud(bl);
	unit_walktoxy(bl, ud->to_x, ud->to_y, 4);
}

void clif_parse_HomMoveTo(int fd, struct map_session_data *sd)
{
	int id = RFIFOL(fd,2); // Mercenary or Homunculus
	struct block_list *bl = NULL;
	short x, y, cmd;

	cmd = RFIFOW(fd,0);
	x = RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0]) * 4 + (RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0] + 1) >> 6);
	y = ((RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0]+1) & 0x3f) << 4) + (RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0] + 2) >> 4);

	if( sd->md && sd->md->bl.id == id )
		bl = &sd->md->bl; // Moving Mercenary
	else if( merc_is_hom_active(sd->hd) && sd->hd->bl.id == id )
		bl = &sd->hd->bl; // Moving Homunculus
	else return;

	unit_walktoxy(bl, x, y, 4);
}

void clif_parse_HomAttack(int fd,struct map_session_data *sd)
{
	struct block_list *bl = NULL;
	int id = RFIFOL(fd,2),
		target_id = RFIFOL(fd,6),
		action_type = RFIFOB(fd,10);

	if( merc_is_hom_active(sd->hd) && sd->hd->bl.id == id )
		bl = &sd->hd->bl;
	else if( sd->md && sd->md->bl.id == id )
		bl = &sd->md->bl;
	else return;

	unit_stop_attack(bl);
	unit_attack(bl, target_id, action_type != 0);
}

void clif_parse_HomMenu(int fd, struct map_session_data *sd)
{	//[orn]
	int cmd;

	cmd = RFIFOW(fd,0);

	if(!merc_is_hom_active(sd->hd))
		return;

	merc_menu(sd,RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0]));
}

void clif_parse_AutoRevive(int fd, struct map_session_data *sd)
{
	int item_position = pc_search_inventory(sd, 7621);

	if (item_position < 0)
		return;

	if (sd->sc.data[SC_HELLPOWER]) //Cannot res while under the effect of SC_HELLPOWER.
		return;

	if (!status_revive(&sd->bl, 100, 100))
		return;
	
	clif_skill_nodamage(&sd->bl,&sd->bl,ALL_RESURRECTION,4,1);
	pc_delitem(sd, item_position, 1, 0, 1);
}


/// Information about character's status values (ZC_ACK_STATUS_GM)
/// 0214 <str>.B <standardStr>.B <agi>.B <standardAgi>.B <vit>.B <standardVit>.B
///      <int>.B <standardInt>.B <dex>.B <standardDex>.B <luk>.B <standardLuk>.B
///      <attPower>.W <refiningPower>.W <max_mattPower>.W <min_mattPower>.W
///      <itemdefPower>.W <plusdefPower>.W <mdefPower>.W <plusmdefPower>.W
///      <hitSuccessValue>.W <avoidSuccessValue>.W <plusAvoidSuccessValue>.W
///      <criticalSuccessValue>.W <ASPD>.W <plusASPD>.W
void clif_check(int fd, struct map_session_data* pl_sd)
{
	WFIFOHEAD(fd,packet_len(0x214));
	WFIFOW(fd, 0) = 0x214;
	WFIFOB(fd, 2) = min(pl_sd->status.str, UINT8_MAX);
	WFIFOB(fd, 3) = pc_need_status_point(pl_sd, SP_STR, 1);
	WFIFOB(fd, 4) = min(pl_sd->status.agi, UINT8_MAX);
	WFIFOB(fd, 5) = pc_need_status_point(pl_sd, SP_AGI, 1);
	WFIFOB(fd, 6) = min(pl_sd->status.vit, UINT8_MAX);
	WFIFOB(fd, 7) = pc_need_status_point(pl_sd, SP_VIT, 1);
	WFIFOB(fd, 8) = min(pl_sd->status.int_, UINT8_MAX);
	WFIFOB(fd, 9) = pc_need_status_point(pl_sd, SP_INT, 1);
	WFIFOB(fd,10) = min(pl_sd->status.dex, UINT8_MAX);
	WFIFOB(fd,11) = pc_need_status_point(pl_sd, SP_DEX, 1);
	WFIFOB(fd,12) = min(pl_sd->status.luk, UINT8_MAX);
	WFIFOB(fd,13) = pc_need_status_point(pl_sd, SP_LUK, 1);
	WFIFOW(fd,14) = pl_sd->battle_status.batk+pl_sd->battle_status.rhw.atk+pl_sd->battle_status.lhw.atk;
	WFIFOW(fd,16) = pl_sd->battle_status.rhw.atk2+pl_sd->battle_status.lhw.atk2;
	WFIFOW(fd,18) = pl_sd->battle_status.matk_max;
	WFIFOW(fd,20) = pl_sd->battle_status.matk_min;
	WFIFOW(fd,22) = pl_sd->battle_status.def;
	WFIFOW(fd,24) = pl_sd->battle_status.def2;
	WFIFOW(fd,26) = pl_sd->battle_status.mdef;
	WFIFOW(fd,28) = pl_sd->battle_status.mdef2;
	WFIFOW(fd,30) = pl_sd->battle_status.hit;
	WFIFOW(fd,32) = pl_sd->battle_status.flee;
	WFIFOW(fd,34) = pl_sd->battle_status.flee2/10;
	WFIFOW(fd,36) = pl_sd->battle_status.cri/10;
	WFIFOW(fd,38) = (2000-pl_sd->battle_status.amotion)/10;  // aspd
	WFIFOW(fd,40) = 0;  // FIXME: What is 'plusASPD' supposed to be?
	WFIFOSET(fd,packet_len(0x214));
}


/// Request character's status values (CZ_REQ_STATUS_GM)
/// /check <char name>
/// 0213 <char name>.24B
void clif_parse_Check(int fd, struct map_session_data *sd)
{
	char charname[NAME_LENGTH];
	struct map_session_data* pl_sd;

	if( pc_isGM(sd) < battle_config.gm_check_minlevel )
	{
		return;
	}

	safestrncpy(charname, (const char*)RFIFOP(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]), sizeof(charname));

	if( ( pl_sd = map_nick2sd(charname) ) == NULL || pc_isGM(sd) < pc_isGM(pl_sd) )
	{
		return;
	}

	clif_check(fd, pl_sd);
}

#ifndef TXT_ONLY
/*==========================================
 * MAIL SYSTEM
 * By Zephyrus
 *==========================================*/

/*------------------------------------------
 * Reply to Set Attachment operation
 * 0 : Successfully attached item to mail
 * 1 : Fail to set the attachment
 *------------------------------------------*/
static void clif_Mail_setattachment(int fd, int index, uint8 flag)
{
	WFIFOHEAD(fd,packet_len(0x255));
	WFIFOW(fd,0) = 0x255;
	WFIFOW(fd,2) = index;
	WFIFOB(fd,4) = flag;
	WFIFOSET(fd,packet_len(0x255));
}

/*------------------------------------------
 * Reply to Get Attachment operation
 * 0 : Successfully added attachment to Inventory
 * 1 : Fail to set the item to inventory
 * 2 : Weight problems
 *------------------------------------------*/
void clif_Mail_getattachment(int fd, uint8 flag)
{
	WFIFOHEAD(fd,packet_len(0x245));
	WFIFOW(fd,0) = 0x245;
	WFIFOB(fd,2) = flag;
	WFIFOSET(fd,packet_len(0x245));
}

/*------------------------------------------
 * Send Mail ack
 * 0 : Message Send Ok
 * 1 : Recipient does not exist
 *------------------------------------------*/
void clif_Mail_send(int fd, bool fail)
{
	WFIFOHEAD(fd,packet_len(0x249));
	WFIFOW(fd,0) = 0x249;
	WFIFOB(fd,2) = fail;
	WFIFOSET(fd,packet_len(0x249));
}

/*------------------------------------------
 * Delete Mail ack
 * 0 : Delete ok
 * 1 : Delete failed
 *------------------------------------------*/
void clif_Mail_delete(int fd, int mail_id, short fail)
{
	WFIFOHEAD(fd, packet_len(0x257));
	WFIFOW(fd,0) = 0x257;
	WFIFOL(fd,2) = mail_id;
	WFIFOW(fd,6) = fail;
	WFIFOSET(fd, packet_len(0x257));
}

/*------------------------------------------
 * Return Mail ack
 * 0 : Mail returned to the sender
 * 1 : The mail does not exist
 *------------------------------------------*/
void clif_Mail_return(int fd, int mail_id, short fail)
{
	WFIFOHEAD(fd,packet_len(0x274));
	WFIFOW(fd,0) = 0x274;
	WFIFOL(fd,2) = mail_id;
	WFIFOW(fd,6) = fail;
	WFIFOSET(fd,packet_len(0x274));
}

/*------------------------------------------
 * You have New Mail
 *------------------------------------------*/
void clif_Mail_new(int fd, int mail_id, const char *sender, const char *title)
{
	WFIFOHEAD(fd,packet_len(0x24a));
	WFIFOW(fd,0) = 0x24a;
	WFIFOL(fd,2) = mail_id;
	safestrncpy((char*)WFIFOP(fd,6), sender, NAME_LENGTH);
	safestrncpy((char*)WFIFOP(fd,30), title, MAIL_TITLE_LENGTH);
	WFIFOSET(fd,packet_len(0x24a));
}

/*------------------------------------------
 * Handles Mail Window on Client
 * flag : 0 open | 1 close
 *------------------------------------------*/
void clif_Mail_window(int fd, int flag)
{
	WFIFOHEAD(fd,packet_len(0x260));
	WFIFOW(fd,0) = 0x260;
	WFIFOL(fd,2) = flag;
	WFIFOSET(fd,packet_len(0x260));
}

/*------------------------------------------
 * Send Inbox Data to Client
 *------------------------------------------*/
void clif_Mail_refreshinbox(struct map_session_data *sd)
{
	int fd = sd->fd;
	struct mail_data *md = &sd->mail.inbox;
	struct mail_message *msg;
	int len, i, j;

	len = 8 + (73 * md->amount);

	WFIFOHEAD(fd,len);
	WFIFOW(fd,0) = 0x240;
	WFIFOW(fd,2) = len;
	WFIFOL(fd,4) = md->amount;
	for( i = j = 0; i < MAIL_MAX_INBOX && j < md->amount; i++ )
	{
		msg = &md->msg[i];
		if (msg->id < 1)
			continue;

		WFIFOL(fd,8+73*j) = msg->id;
		memcpy(WFIFOP(fd,12+73*j), msg->title, MAIL_TITLE_LENGTH);
		WFIFOB(fd,52+73*j) = (msg->status != MAIL_UNREAD); // 0: unread, 1: read
		memcpy(WFIFOP(fd,53+73*j), msg->send_name, NAME_LENGTH);
		WFIFOL(fd,77+73*j) = (uint32)msg->timestamp;
		j++;
	}
	WFIFOSET(fd,len);

	if( md->full )
	{
		char output[100];
		sprintf(output, "Inbox is full (Max %d). Delete some mails.", MAIL_MAX_INBOX);
		clif_disp_onlyself(sd, output, strlen(output));
	}
}

/*------------------------------------------
 * Client Request Inbox List
 *------------------------------------------*/
void clif_parse_Mail_refreshinbox(int fd, struct map_session_data *sd)
{
	struct mail_data* md = &sd->mail.inbox;

	if( md->amount < MAIL_MAX_INBOX && (md->full || sd->mail.changed) )
		intif_Mail_requestinbox(sd->status.char_id, 1);
	else
		clif_Mail_refreshinbox(sd);

	mail_removeitem(sd, 0);
	mail_removezeny(sd, 0);
}

/*------------------------------------------
 * Read Message
 *------------------------------------------*/
void clif_Mail_read(struct map_session_data *sd, int mail_id)
{
	int i, fd = sd->fd;

	ARR_FIND(0, MAIL_MAX_INBOX, i, sd->mail.inbox.msg[i].id == mail_id);
	if( i == MAIL_MAX_INBOX )
	{
		clif_Mail_return(sd->fd, mail_id, 1); // Mail doesn't exist
		ShowWarning("clif_parse_Mail_read: char '%s' trying to read a message not the inbox.\n", sd->status.name);
		return;
	}
	else
	{
		struct mail_message *msg = &sd->mail.inbox.msg[i];
		struct item *item = &msg->item;
		struct item_data *data;
		int msg_len = strlen(msg->body), len;

		if( msg_len == 0 ) {
			strcpy(msg->body, "(no message)");
			msg_len = strlen(msg->body);
		}

		len = 101 + msg_len;

		WFIFOHEAD(fd,len);
		WFIFOW(fd,0) = 0x242;
		WFIFOW(fd,2) = len;
		WFIFOL(fd,4) = msg->id;
		safestrncpy((char*)WFIFOP(fd,8), msg->title, MAIL_TITLE_LENGTH + 1);
		safestrncpy((char*)WFIFOP(fd,48), msg->send_name, NAME_LENGTH + 1);
		WFIFOL(fd,72) = 0;
		WFIFOL(fd,76) = msg->zeny;

		if( item->nameid && (data = itemdb_exists(item->nameid)) != NULL )
		{
			WFIFOL(fd,80) = item->amount;
			WFIFOW(fd,84) = (data->view_id)?data->view_id:item->nameid;
			WFIFOW(fd,86) = data->type;
			WFIFOB(fd,88) = item->identify;
			WFIFOB(fd,89) = item->attribute;
			WFIFOB(fd,90) = item->refine;
			WFIFOW(fd,91) = item->card[0];
			WFIFOW(fd,93) = item->card[1];
			WFIFOW(fd,95) = item->card[2];
			WFIFOW(fd,97) = item->card[3];
		}
		else // no item, set all to zero
			memset(WFIFOP(fd,80), 0x00, 19);

		WFIFOB(fd,99) = (unsigned char)msg_len;
		safestrncpy((char*)WFIFOP(fd,100), msg->body, msg_len + 1);
		WFIFOSET(fd,len);

		if (msg->status == MAIL_UNREAD) {
			msg->status = MAIL_READ;
			intif_Mail_read(mail_id);
			clif_parse_Mail_refreshinbox(fd, sd);
		}
	}
}

void clif_parse_Mail_read(int fd, struct map_session_data *sd)
{
	int mail_id = RFIFOL(fd,2);

	if( mail_id <= 0 )
		return;
	if( mail_invalid_operation(sd) )
		return;

	clif_Mail_read(sd, RFIFOL(fd,2));
}

/*------------------------------------------
 * Get Attachment from Message
 *------------------------------------------*/
void clif_parse_Mail_getattach(int fd, struct map_session_data *sd)
{
	int mail_id = RFIFOL(fd,2);
	int i;
	bool fail = false;

	if( mail_id <= 0 )
		return;
	if( mail_invalid_operation(sd) )
		return;

	ARR_FIND(0, MAIL_MAX_INBOX, i, sd->mail.inbox.msg[i].id == mail_id);
	if( i == MAIL_MAX_INBOX )
		return;

	if( sd->mail.inbox.msg[i].zeny < 1 && (sd->mail.inbox.msg[i].item.nameid < 1 || sd->mail.inbox.msg[i].item.amount < 1) )
		return;

	if( sd->mail.inbox.msg[i].item.nameid > 0 )
	{
		struct item_data *data;
		unsigned int weight;

		if ((data = itemdb_exists(sd->mail.inbox.msg[i].item.nameid)) == NULL)
			return;

		switch( pc_checkadditem(sd, data->nameid, sd->mail.inbox.msg[i].item.amount) )
		{
			case ADDITEM_NEW:
				fail = ( pc_inventoryblank(sd) == 0 );
				break;
			case ADDITEM_OVERAMOUNT:
				fail = true;
		}

		if( fail )
		{
			clif_Mail_getattachment(fd, 1);
			return;
		}

		weight = data->weight * sd->mail.inbox.msg[i].item.amount;
		if( sd->weight + weight > sd->max_weight )
		{
			clif_Mail_getattachment(fd, 2);
			return;
		}
	}

	sd->mail.inbox.msg[i].zeny = 0;
	memset(&sd->mail.inbox.msg[i].item, 0, sizeof(struct item));
	clif_Mail_read(sd, mail_id);

	intif_Mail_getattach(sd->status.char_id, mail_id);
}

/*------------------------------------------
 * Delete Message
 *------------------------------------------*/
void clif_parse_Mail_delete(int fd, struct map_session_data *sd)
{
	int mail_id = RFIFOL(fd,2);
	int i;

	if( mail_id <= 0 )
		return;
	if( mail_invalid_operation(sd) )
		return;

	ARR_FIND(0, MAIL_MAX_INBOX, i, sd->mail.inbox.msg[i].id == mail_id);
	if (i < MAIL_MAX_INBOX)
	{
		struct mail_message *msg = &sd->mail.inbox.msg[i];

		if( (msg->item.nameid > 0 && msg->item.amount > 0) || msg->zeny > 0 )
		{// can't delete mail without removing attachment first
			clif_Mail_delete(sd->fd, mail_id, 1);
			return;
		}
		
		intif_Mail_delete(sd->status.char_id, mail_id);
	}
}

/*------------------------------------------
 * Return Mail Message
 *------------------------------------------*/
void clif_parse_Mail_return(int fd, struct map_session_data *sd)
{
	int mail_id = RFIFOL(fd,2);
	int i;

	if( mail_id <= 0 )
		return;
	if( mail_invalid_operation(sd) )
		return;

	ARR_FIND(0, MAIL_MAX_INBOX, i, sd->mail.inbox.msg[i].id == mail_id);
	if( i < MAIL_MAX_INBOX && sd->mail.inbox.msg[i].send_id != 0 )
		intif_Mail_return(sd->status.char_id, mail_id);
	else
		clif_Mail_return(sd->fd, mail_id, 1);
}

/*------------------------------------------
 * Set Attachment
 *------------------------------------------*/
void clif_parse_Mail_setattach(int fd, struct map_session_data *sd)
{
	int idx = RFIFOW(fd,2);
	int amount = RFIFOL(fd,4);
	unsigned char flag;

	if (idx < 0 || amount < 0)
		return;

	flag = mail_setitem(sd, idx, amount);
	clif_Mail_setattachment(fd,idx,flag);
}

/*------------------------------------------
 * Mail Window Operation
 * S 0246 <flag>.W
 * 0 : Switch to 'new mail' window, or Close mailbox
 * 1 : ???
 * 2 : Zeny entering start
 *------------------------------------------*/
void clif_parse_Mail_winopen(int fd, struct map_session_data *sd)
{
	int flag = RFIFOW(fd,2);

	if (flag == 0 || flag == 1)
		mail_removeitem(sd, 0);
	if (flag == 0 || flag == 2)
		mail_removezeny(sd, 0);
}

/*------------------------------------------
 * Send Mail
 * S 0248 <packet len>.w <nick>.24B <title>.40B <body len>.B <message>.?B 00
 *------------------------------------------*/
void clif_parse_Mail_send(int fd, struct map_session_data *sd)
{
	struct mail_message msg;
	int body_len;

	if( sd->state.trading )
		return;

	if( RFIFOW(fd,2) < 69 ) {
		ShowWarning("Invalid Msg Len from account %d.\n", sd->status.account_id);
		return;
	}

	if( DIFF_TICK(sd->cansendmail_tick, gettick()) > 0 )
	{
		clif_displaymessage(sd->fd,"Cannot send mails too fast!!.");
		clif_Mail_send(fd, true); // fail
		return;
	}

	body_len = RFIFOB(fd,68);

	if (body_len > MAIL_BODY_LENGTH)
		body_len = MAIL_BODY_LENGTH;

	if( !mail_setattachment(sd, &msg) )
	{ // Invalid Append condition
		clif_Mail_send(sd->fd, true); // fail
		mail_removeitem(sd,0);
		mail_removezeny(sd,0);
		return;
	}

	msg.id = 0; // id will be assigned by charserver
	msg.send_id = sd->status.char_id;
	msg.dest_id = 0; // will attempt to resolve name
	safestrncpy(msg.send_name, sd->status.name, NAME_LENGTH);
	safestrncpy(msg.dest_name, (char*)RFIFOP(fd,4), NAME_LENGTH);
	safestrncpy(msg.title, (char*)RFIFOP(fd,28), MAIL_TITLE_LENGTH);
	
	if (body_len)
		safestrncpy(msg.body, (char*)RFIFOP(fd,69), body_len + 1);
	else
		memset(msg.body, 0x00, MAIL_BODY_LENGTH);
	
	msg.timestamp = time(NULL);
	if( !intif_Mail_send(sd->status.account_id, &msg) )
		mail_deliveryfail(sd, &msg);

	sd->cansendmail_tick = gettick() + 1000; // 1 Second flood Protection
}

/*==========================================
 * AUCTION SYSTEM
 * By Zephyrus
 *==========================================*/


/// Opens/closes the auction window (ZC_AUCTION_WINDOWS)
/// 025f <type>.L
/// type:
///     0 = open
///     1 = close
void clif_Auction_openwindow(struct map_session_data *sd)
{
	int fd = sd->fd;

	if( sd->state.storage_flag || sd->state.vending || sd->state.buyingstore || sd->state.trading )
		return;

	WFIFOHEAD(fd,packet_len(0x25f));
	WFIFOW(fd,0) = 0x25f;
	WFIFOL(fd,2) = 0;
	WFIFOSET(fd,packet_len(0x25f));
}


/// Returns auction item search results (ZC_AUCTION_ITEM_REQ_SEARCH)
/// 0252 <packet len>.W <pages>.L <count>.L { <auction id>.L <seller name>.24B <name id>.W <type>.L <amount>.W <identified>.B <damaged>.B <refine>.B <card1>.W <card2>.W <card3>.W <card4>.W <now price>.L <max price>.L <buyer name>.24B <delete time>.L }*
void clif_Auction_results(struct map_session_data *sd, short count, short pages, uint8 *buf)
{
	int i, fd = sd->fd, len = sizeof(struct auction_data);
	struct auction_data auction;
	struct item_data *item;
	int k;

	WFIFOHEAD(fd,12 + (count * 83));
	WFIFOW(fd,0) = 0x252;
	WFIFOW(fd,2) = 12 + (count * 83);
	WFIFOL(fd,4) = pages;
	WFIFOL(fd,8) = count;

	for( i = 0; i < count; i++ )
	{
		memcpy(&auction, RBUFP(buf,i * len), len);
		k = 12 + (i * 83);

		WFIFOL(fd,k) = auction.auction_id;
		safestrncpy((char*)WFIFOP(fd,4+k), auction.seller_name, NAME_LENGTH);

		if( (item = itemdb_exists(auction.item.nameid)) != NULL && item->view_id > 0 )
			WFIFOW(fd,28+k) = item->view_id;
		else
			WFIFOW(fd,28+k) = auction.item.nameid;

		WFIFOL(fd,30+k) = auction.type;
		WFIFOW(fd,34+k) = auction.item.amount; // Always 1
		WFIFOB(fd,36+k) = auction.item.identify;
		WFIFOB(fd,37+k) = auction.item.attribute;
		WFIFOB(fd,38+k) = auction.item.refine;
		WFIFOW(fd,39+k) = auction.item.card[0];
		WFIFOW(fd,41+k) = auction.item.card[1];
		WFIFOW(fd,43+k) = auction.item.card[2];
		WFIFOW(fd,45+k) = auction.item.card[3];
		WFIFOL(fd,47+k) = auction.price;
		WFIFOL(fd,51+k) = auction.buynow;
		safestrncpy((char*)WFIFOP(fd,55+k), auction.buyer_name, NAME_LENGTH);
		WFIFOL(fd,79+k) = (uint32)auction.timestamp;
	}
	WFIFOSET(fd,WFIFOW(fd,2));
}

static void clif_Auction_setitem(int fd, int index, bool fail)
{
	WFIFOHEAD(fd,packet_len(0x256));
	WFIFOW(fd,0) = 0x256;
	WFIFOW(fd,2) = index;
	WFIFOB(fd,4) = fail;
	WFIFOSET(fd,packet_len(0x256));
}

void clif_parse_Auction_cancelreg(int fd, struct map_session_data *sd)
{
	if( sd->auction.amount > 0 )
		clif_additem(sd, sd->auction.index, sd->auction.amount, 0);

	sd->auction.amount = 0;
}

void clif_parse_Auction_setitem(int fd, struct map_session_data *sd)
{
	int idx = RFIFOW(fd,2) - 2;
	int amount = RFIFOL(fd,4); // Always 1
	struct item_data *item;

	if( sd->auction.amount > 0 )
		sd->auction.amount = 0;

	if( idx < 0 || idx >= MAX_INVENTORY )
	{
		ShowWarning("Character %s trying to set invalid item index in auctions.\n", sd->status.name);
		return;
	}

	if( amount != 1 || amount < 0 || amount > sd->status.inventory[idx].amount )
	{ // By client, amount is allways set to 1. Maybe this is a future implementation.
		ShowWarning("Character %s trying to set invalid amount in auctions.\n", sd->status.name);
		return;
	}

	if( (item = itemdb_exists(sd->status.inventory[idx].nameid)) != NULL && !(item->type == IT_ARMOR || item->type == IT_PETARMOR || item->type == IT_WEAPON || item->type == IT_CARD || item->type == IT_ETC) )
	{ // Consumible or pets are not allowed
		clif_Auction_setitem(sd->fd, idx, true);
		return;
	}
	
	if( sd->state.secure_items )
	{
		clif_Auction_setitem(sd->fd, idx, true);
		clif_displaymessage(sd->fd, "You can't open auction. Blocked with @security");
		return;
	}

	if( !pc_candrop(sd, &sd->status.inventory[idx]) || !sd->status.inventory[idx].identify )
	{ // Quest Item or something else
		clif_Auction_setitem(sd->fd, idx, true);
		return;
	}
	
	sd->auction.index = idx;
	sd->auction.amount = amount;
	clif_Auction_setitem(fd, idx + 2, false);
}

/// Result from an auction action (ZC_AUCTION_RESULT)
/// 0250 <result>.B
/// result:
///     0 = You have failed to bid into the auction
///     1 = You have successfully bid in the auction
///     2 = The auction has been canceled
///     3 = An auction with at least one bidder cannot be canceled
///     4 = You cannot register more than 5 items in an auction at a time
///     5 = You do not have enough Zeny to pay the Auction Fee
///     6 = You have won the auction
///     7 = You have failed to win the auction
///     8 = You do not have enough Zeny
///     9 = You cannot place more than 5 bids at a time
void clif_Auction_message(int fd, unsigned char flag)
{
	WFIFOHEAD(fd,packet_len(0x250));
	WFIFOW(fd,0) = 0x250;
	WFIFOB(fd,2) = flag;
	WFIFOSET(fd,packet_len(0x250));
}


/// Result of the auction close request (ZC_AUCTION_ACK_MY_SELL_STOP)
/// 025e <result>.W
/// result:
///     0 = You have ended the auction
///     1 = You cannot end the auction
///     2 = Bid number is incorrect
void clif_Auction_close(int fd, unsigned char flag)
{
	WFIFOHEAD(fd,packet_len(0x25e));
	WFIFOW(fd,0) = 0x25d;  // BUG: The client identifies this packet as 0x25d (CZ_AUCTION_REQ_MY_SELL_STOP)
	WFIFOW(fd,2) = flag;
	WFIFOSET(fd,packet_len(0x25e));
}

void clif_parse_Auction_register(int fd, struct map_session_data *sd)
{
	struct auction_data auction;
	struct item_data *item;

	auction.price = RFIFOL(fd,2);
	auction.buynow = RFIFOL(fd,6);
	auction.hours = RFIFOW(fd,10);

	// Invalid Situations...
	if( sd->auction.amount < 1 )
	{
		ShowWarning("Character %s trying to register auction without item.\n", sd->status.name);
		return;
	}

	if( auction.price >= auction.buynow )
	{
		ShowWarning("Character %s trying to alter auction prices.\n", sd->status.name);
		return;
	}

	if( auction.hours < 1 || auction.hours > 48 )
	{
		ShowWarning("Character %s trying to enter an invalid time for auction.\n", sd->status.name);
		return;
	}

	// Auction checks...
	if( sd->status.zeny < (auction.hours * battle_config.auction_feeperhour) )
	{
		clif_Auction_message(fd, 5); // You do not have enough zeny to pay the Auction Fee.
		return;
	}

	if( auction.buynow > battle_config.auction_maximumprice )
	{ // Zeny Limits
		auction.buynow = battle_config.auction_maximumprice;
		if( auction.price >= auction.buynow )
			auction.price = auction.buynow - 1;
	}

	auction.auction_id = 0;
	auction.seller_id = sd->status.char_id;
	safestrncpy(auction.seller_name, sd->status.name, NAME_LENGTH);
	auction.buyer_id = 0;
	memset(&auction.buyer_name, '\0', NAME_LENGTH);

	if( sd->status.inventory[sd->auction.index].nameid == 0 || sd->status.inventory[sd->auction.index].amount < sd->auction.amount )
	{
		clif_Auction_message(fd, 2); // The auction has been canceled
		return;
	}

	if( (item = itemdb_exists(sd->status.inventory[sd->auction.index].nameid)) == NULL )
	{ // Just in case
		clif_Auction_message(fd, 2); // The auction has been canceled
		return;
	}

	safestrncpy(auction.item_name, item->jname, ITEM_NAME_LENGTH);
	auction.type = item->type;
	memcpy(&auction.item, &sd->status.inventory[sd->auction.index], sizeof(struct item));
	auction.item.amount = 1;
	auction.timestamp = 0;

	if( !intif_Auction_register(&auction) )
		clif_Auction_message(fd, 4); // No Char Server? lets say something to the client
	else
	{
		pc_delitem(sd, sd->auction.index, sd->auction.amount, 1, 6);
		sd->auction.amount = 0;
		pc_payzeny(sd, auction.hours * battle_config.auction_feeperhour);
	}
}

void clif_parse_Auction_cancel(int fd, struct map_session_data *sd)
{
	unsigned int auction_id = RFIFOL(fd,2);

	intif_Auction_cancel(sd->status.char_id, auction_id);
}

void clif_parse_Auction_close(int fd, struct map_session_data *sd)
{
	unsigned int auction_id = RFIFOL(fd,2);

	intif_Auction_close(sd->status.char_id, auction_id);
}

void clif_parse_Auction_bid(int fd, struct map_session_data *sd)
{
	unsigned int auction_id = RFIFOL(fd,2);
	int bid = RFIFOL(fd,6);

	if( !pc_can_give_items(pc_isGM(sd)) )
	{ //They aren't supposed to give zeny [Inkfish]
		clif_displaymessage(sd->fd, msg_txt(246));
		return;
	}

	if( bid <= 0 )
		clif_Auction_message(fd, 0); // You have failed to bid into the auction
	else if( bid > sd->status.zeny )
		clif_Auction_message(fd, 8); // You do not have enough zeny
	else
	{
		pc_payzeny(sd, bid);
		intif_Auction_bid(sd->status.char_id, sd->status.name, auction_id, bid);
	}
}

/*------------------------------------------
 * Auction Search
 * S 0251 <search type>.w <search price>.l <search text>.24B <page number>.w
 * Search Type: 0 Armor 1 Weapon 2 Card 3 Misc 4 By Text 5 By Price 6 Sell 7 Buy
 *------------------------------------------*/
void clif_parse_Auction_search(int fd, struct map_session_data* sd)
{
	char search_text[NAME_LENGTH];
	short type = RFIFOW(fd,2), page = RFIFOW(fd,32);
	int price = RFIFOL(fd,4);

	clif_parse_Auction_cancelreg(fd, sd);
	
	safestrncpy(search_text, (char*)RFIFOP(fd,8), NAME_LENGTH);
	intif_Auction_requestlist(sd->status.char_id, type, price, search_text, page);
}

void clif_parse_Auction_buysell(int fd, struct map_session_data* sd)
{
	short type = RFIFOW(fd,2) + 6;
	clif_parse_Auction_cancelreg(fd, sd);

	intif_Auction_requestlist(sd->status.char_id, type, 0, "", 1);
}

#endif

/*==========================================
 * CASH/POINT SHOP
 *==========================================*/

static int clif_min_badges(struct map_session_data *sd)
{
	int i, i7828 = 0, i7829 = 0, i7773 = 0;
	if( !sd ) return 0;

	i = pc_search_inventory(sd,7828);
	if( i >= 0 ) i7828 = sd->status.inventory[i].amount;
	i = pc_search_inventory(sd,7829);
	if( i >= 0 ) i7829 = sd->status.inventory[i].amount;
	i = pc_search_inventory(sd,7773);
	if( i >= 0 ) i7773 = sd->status.inventory[i].amount;

	return min(min(i7828,i7829),i7773);
}

/// List of items offered in a cash shop (ZC_PC_CASH_POINT_ITEMLIST)
/// 0287 <packet len>.W <cash point>.L { <sell price>.L <discount price>.L <item type>.B <name id>.W }*
/// 0287 <packet len>.W <cash point>.L <kafra point>.L { <sell price>.L <discount price>.L <item type>.B <name id>.W }* (PACKETVER >= 20070711)
void clif_cashshop_show(struct map_session_data *sd, struct npc_data *nd)
{
	int fd,i,type = 0;
	unsigned int val;
	bool Premium;
	int value1 = 0, // Cash Points client slot
		value2 = 0; // Free Points client slot

	nullpo_retv(sd);
	nullpo_retv(nd);

	fd = sd->fd;
	sd->npc_shopid = nd->bl.id;
	Premium = pc_isPremium(sd) && battle_config.premium_discount > 0;

	WFIFOHEAD(fd, 200 * 11 + 12);
	WFIFOW(fd,0) = 0x287;
	WFIFOW(fd,2) = 12 + nd->u.shop.count*11;

	if( nd->subtype == SPSHOP )
	{
		if( nd->cashitem < 0 )
		{
			value1 = clif_min_badges(sd);
			type = 1; // BG Shop Flag
		}
		else if( (i = pc_search_inventory(sd,nd->cashitem)) >= 0 )
		{
			value1 = sd->status.inventory[i].amount;
			type = 2; // Item Shop Flag
		}
	}
	else
	{
		value1 = pc_readregistry(sd,nd->u.shop.cash_var,nd->u.shop.cash_vartype);
		value2 = (nd->u.shop.point_vartype != -1) ? pc_readregistry(sd,nd->u.shop.point_var,nd->u.shop.point_vartype) : 0;
	}

	WFIFOL(fd,4) = value1; // Cash Points
	WFIFOL(fd,8) = value2; // Kafra Points

	for( i = 0; i < nd->u.shop.count; i++ )
	{
		struct item_data* id = itemdb_search(nd->u.shop.shop_item[i].nameid);
		val = nd->u.shop.shop_item[i].value;

		WFIFOL(fd,12+i*11) = val;
		if( Premium && nd->subtype == CASHSHOP )
		{ // Discount Price for Premium Account
			val -= nd->u.shop.shop_item[i].value * battle_config.premium_discount / 100;
			if( val <= 0 ) val = 1;
		}

		WFIFOL(fd,16+i*11) = val;
		WFIFOB(fd,20+i*11) = itemtype(id->type);
		WFIFOW(fd,21+i*11) = ( id->view_id > 0 ) ? id->view_id : id->nameid;
	}
	WFIFOSET(fd,WFIFOW(fd,2));

	if( type > 0 )
	{
		char output[128];
		if( type == 1 )
			sprintf(output,"This is a Battleground Shop. Bravery, Honor and Valour badges in the same amount are required to exchange items.");
		else
			sprintf(output,"This is a Item Exchange Shop. You need %s (%d) to exchange items.", itemdb_search(nd->cashitem)->jname, nd->cashitem);

		clif_displaymessage(fd, output);
	}
}

/// Cashshop Buy Ack (ZC_PC_CASH_POINT_UPDATE)
/// S 0289 <cash point>.L <error>.W
/// S 0289 <cash point>.L <kafra point>.L <error>.W (PACKETVER >= 20070711)
///
/// @param error
/// 0: The deal has successfully completed. (ERROR_TYPE_NONE)
/// 1: The Purchase has failed because the NPC does not exist. (ERROR_TYPE_NPC)
/// 2: The Purchase has failed because the Kafra Shop System is not working correctly. (ERROR_TYPE_SYSTEM)
/// 3: You are over your Weight Limit. (ERROR_TYPE_INVENTORY_WEIGHT)
/// 4: You cannot purchase items while you are in a trade. (ERROR_TYPE_EXCHANGE)
/// 5: The Purchase has failed because the Item Information was incorrect. (ERROR_TYPE_ITEM_ID)
/// 6: You do not have enough Kafra Credit Points. (ERROR_TYPE_MONEY)
/// 7: You can purchase up to 10 items.
/// 8: Some items could not be purchased.
void clif_cashshop_ack(struct map_session_data* sd, struct npc_data* nd, int error)
{
	int fd = sd->fd, i;
	int value1 = 0, // Cash Points client slot
		value2 = 0; // Free Points client slot

	if( !nd || (nd->subtype != CASHSHOP && nd->subtype != SPSHOP) )
		error = 1;
	else
	{
		if( nd->subtype == SPSHOP )
		{
			if( nd->cashitem < 0 )
				value1 = clif_min_badges(sd);
			else if( (i = pc_search_inventory(sd,nd->cashitem)) >= 0 )
				value1 = sd->status.inventory[i].amount;
		}
		else
		{
			value1 = pc_readregistry(sd,nd->u.shop.cash_var,nd->u.shop.cash_vartype);
			value2 = (nd->u.shop.point_vartype != -1) ? pc_readregistry(sd,nd->u.shop.point_var,nd->u.shop.point_vartype) : 0;
		}
	}

	WFIFOHEAD(fd, packet_len(0x289));
	WFIFOW(fd,0) = 0x289;
	WFIFOL(fd,2) = value1;
#if PACKETVER < 20070711
	WFIFOW(fd,6) = TOW(error);
#else
	WFIFOL(fd,6) = value2;
	WFIFOW(fd,10) = TOW(error);
#endif
	WFIFOSET(fd, packet_len(0x289));
}

/// Request to buy item(s) from cash shop (CZ_PC_BUY_CASH_POINT_ITEM).
/// 0288 <name id>.W <amount>.W
/// 0288 <name id>.W <amount>.W <kafra points>.L (PACKETVER >= 20070711)
/// 0288 <packet len>.W <kafra points>.L <count>.W { <amount>.W <name id>.W }.4B*count (PACKETVER >= 20100803)
void clif_parse_cashshop_buy(int fd, struct map_session_data *sd)
{
	int fail = 0;
	struct npc_data *nd;
	nullpo_retv(sd);

	if( (nd = (struct npc_data *)map_id2bl(sd->npc_shopid)) == NULL )
		return;

	if( sd->state.trading || !sd->npc_shopid )
		fail = 1;
	else if( sd->state.secure_items )
	{
		clif_displaymessage(sd->fd, "You can't shop. Blocked with @security");
		fail = 1;
	}
	else
	{
#if PACKETVER < 20101116
		short nameid = RFIFOW(fd,2);
		short amount = RFIFOW(fd,4);
		int points = RFIFOL(fd,6);

		fail = npc_cashshop_buy(sd, nameid, amount, points);
#else
		int len = RFIFOW(fd,2);
		int points = RFIFOL(fd,4);
		int count = RFIFOW(fd,8);
		unsigned short* item_list = (unsigned short*)RFIFOP(fd,10);

		if( len < 10 || len != 10 + count * 4)
		{
			ShowWarning("Player %u sent incorrect cash shop buy packet (len %u:%u)!\n", sd->status.char_id, len, 10 + count * 4);
			return;
		}
		fail = npc_cashshop_buylist(sd,points,count,item_list);
#endif
	}

	clif_cashshop_ack(sd,nd,fail);
}

/*==========================================
 * Adoption System
 *==========================================*/

// 0 : "You cannot adopt more than 1 child."
// 1 : "You must be at least character level 70 in order to adopt someone."
// 2 : "You cannot adopt a married person."
void clif_Adopt_reply(struct map_session_data *sd, int type)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,6);
	WFIFOW(fd,0) = 0x0216;
	WFIFOL(fd,2) = type;
	WFIFOSET(fd,6);
}

void clif_Adopt_request(struct map_session_data *sd, struct map_session_data *src, int p_id)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,34);
	WFIFOW(fd,0) = 0x01f6;
	WFIFOL(fd,2) = src->status.account_id;
	WFIFOL(fd,6) = p_id;
	memcpy(WFIFOP(fd,10), src->status.name, NAME_LENGTH);
	WFIFOSET(fd,34);
}

void clif_parse_Adopt_request(int fd, struct map_session_data *sd)
{
	struct map_session_data *tsd = map_id2sd(RFIFOL(fd,2)), *p_sd = map_charid2sd(sd->status.partner_id);

	if( pc_can_Adopt(sd, p_sd, tsd) )
	{
		tsd->adopt_invite = sd->status.account_id;
		clif_Adopt_request(tsd, sd, p_sd->status.account_id);
	}
}

void clif_parse_Adopt_reply(int fd, struct map_session_data *sd)
{
	int p1_id = RFIFOL(fd,2);
	int p2_id = RFIFOL(fd,6);
	int result = RFIFOL(fd,10);
	struct map_session_data* p1_sd = map_id2sd(p1_id);
	struct map_session_data* p2_sd = map_id2sd(p2_id);

	int pid = sd->adopt_invite;
	sd->adopt_invite = 0;

	if( p1_sd == NULL || p2_sd == NULL )
		return; // Both players need to be online

	if( pid != p1_sd->status.account_id )
		return; // Incorrect values

	if( result == 0 )
		return; // Rejected

	pc_adoption(p1_sd, p2_sd, sd);
}

/*==========================================
 * Convex Mirror (ZC_BOSS_INFO)
 * S 0293 <infoType>.B <x>.L <y>.L <minHours>.W <minMinutes>.W <maxHours>.W <maxMinutes>.W  <monster name>.51B
 * infoType:
 *  0 = No boss on this map (BOSS_INFO_NOT).
 *  1 = Boss is alive (position update) (BOSS_INFO_ALIVE).
 *  2 = Boss is alive (initial announce) (BOSS_INFO_ALIVE_WITHMSG).
 *  3 = Boss is dead (BOSS_INFO_DEAD).
 *==========================================*/
void clif_bossmapinfo(int fd, struct mob_data *md, short flag)
{
	WFIFOHEAD(fd,70);
	memset(WFIFOP(fd,0),0,70);
	WFIFOW(fd,0) = 0x0293;

	if( md != NULL )
	{
		if( md->bl.prev != NULL )
		{ // Boss on This Map
			if( flag )
			{
				WFIFOB(fd,2) = 1;
				WFIFOL(fd,3) = md->bl.x;
				WFIFOL(fd,7) = md->bl.y;
			}
			else
				WFIFOB(fd,2) = 2; // First Time
		}
		else if (md->spawn_timer != INVALID_TIMER)
		{ // Boss is Dead
			const struct TimerData * timer_data = get_timer(md->spawn_timer);
			unsigned int seconds;
			int hours, minutes;

			seconds = DIFF_TICK(timer_data->tick, gettick()) / 1000 + 60;
			hours = seconds / (60 * 60);
			seconds = seconds - (60 * 60 * hours);
			minutes = seconds / 60;

			WFIFOB(fd,2) = 3;
			WFIFOW(fd,11) = hours; // Hours
			WFIFOW(fd,13) = minutes; // Minutes
		}
		safestrncpy((char*)WFIFOP(fd,19), md->db->jname, NAME_LENGTH);
	}

	WFIFOSET(fd,70);
}

/*==========================================
 * Requesting equip of a player
 *------------------------------------------*/
void clif_parse_ViewPlayerEquip(int fd, struct map_session_data* sd)
{
	int charid = RFIFOL(fd, 2);
	struct map_session_data* tsd = map_id2sd(charid);
	
	if (!tsd)
		return;

	if( tsd->status.show_equip || (battle_config.gm_viewequip_min_lv && pc_isGM(sd) >= battle_config.gm_viewequip_min_lv) )
		clif_viewequip_ack(sd, tsd);
	else
		clif_viewequip_fail(sd);
}

/*==========================================
 * Equip window (un)tick
 * S 02d8 <zero?>.L <flag>.L
 *------------------------------------------*/
void clif_parse_EquipTick(int fd, struct map_session_data* sd)
{
	bool flag = (bool)RFIFOL(fd,6); // 0=off, 1=on
	sd->status.show_equip = flag;
	clif_equiptickack(sd, flag);
}

/*==========================================
 * Questlog System [Kevin] [Inkfish]
 *------------------------------------------*/
//* 02B1 <packet_len>.W <quest_num>.L { <quest_id>.L <state>.B }.5B*
void clif_quest_send_list(struct map_session_data * sd)
{
	int fd = sd->fd;
	int i;
	int len = sd->avail_quests*5+8;

	WFIFOHEAD(fd,len);
	WFIFOW(fd, 0) = 0x02B1;
	WFIFOW(fd, 2) = len;
	WFIFOL(fd, 4) = sd->avail_quests;

	for( i = 0; i < sd->avail_quests; i++ )
	{
		WFIFOL(fd, i*5+8) = sd->quest_log[i].quest_id;
		WFIFOB(fd, i*5+12) = sd->quest_log[i].state;
	}

	WFIFOSET(fd, len);

}

//* 02B2 <packet_len>.W <quest_num>.L { <quest_id>.L <start time>.L <expire time>.L <num mobs>.W {<mob id>.L <mob count>.W <Mob Name>.24B}.30B[3] }.104B*
void clif_quest_send_mission(struct map_session_data * sd)
{
	int fd = sd->fd;
	int i, j;
	int len = sd->avail_quests*104+8;
	struct mob_db *mob;

	WFIFOHEAD(fd, len);
	WFIFOW(fd, 0) = 0x02B2;
	WFIFOW(fd, 2) = len;
	WFIFOL(fd, 4) = sd->avail_quests;

	for( i = 0; i < sd->avail_quests; i++ )
	{
		WFIFOL(fd, i*104+8) = sd->quest_log[i].quest_id;
		WFIFOL(fd, i*104+12) = sd->quest_log[i].time - quest_db[sd->quest_index[i]].time;
		WFIFOL(fd, i*104+16) = sd->quest_log[i].time;
		WFIFOW(fd, i*104+20) = quest_db[sd->quest_index[i]].num_objectives;

		for( j = 0 ; j < quest_db[sd->quest_index[i]].num_objectives; j++ )
		{
			WFIFOL(fd, i*104+22+j*30) = quest_db[sd->quest_index[i]].mob[j];
			WFIFOW(fd, i*104+26+j*30) = sd->quest_log[i].count[j];
			mob = mob_db(quest_db[sd->quest_index[i]].mob[j]);
			memcpy(WFIFOP(fd, i*104+28+j*30), mob?mob->jname:"NULL", NAME_LENGTH);
		}
	}

	WFIFOSET(fd, len);
}

//* 02B3 <quest_id>.L <state>.B <start time>.L <expire time>.L <num mobs>.W {<mob id>.L <mob count>.W <Mob Name>.24B}.30B[3]
void clif_quest_add(struct map_session_data * sd, struct quest * qd, int index)
{
	int fd = sd->fd;
	int i;
	struct mob_db *mob;

	WFIFOHEAD(fd, packet_len(0x02B3));
	WFIFOW(fd, 0) = 0x02B3;
	WFIFOL(fd, 2) = qd->quest_id;
	WFIFOB(fd, 6) = qd->state;
	WFIFOB(fd, 7) = qd->time - quest_db[index].time;
	WFIFOL(fd, 11) = qd->time;
	WFIFOW(fd, 15) = quest_db[index].num_objectives;

	for( i = 0; i < quest_db[index].num_objectives; i++ )
	{
		WFIFOL(fd, i*30+17) = quest_db[index].mob[i];
		WFIFOW(fd, i*30+21) = qd->count[i];
		mob = mob_db(quest_db[index].mob[i]);
		memcpy(WFIFOP(fd, i*30+23), mob?mob->jname:"NULL", NAME_LENGTH);
	}

	WFIFOSET(fd, packet_len(0x02B3));
}

//* 02B4 <quest_id>.L
void clif_quest_delete(struct map_session_data * sd, int quest_id)
{
	int fd = sd->fd;

	WFIFOHEAD(fd, packet_len(0x02B4));
	WFIFOW(fd, 0) = 0x02B4;
	WFIFOL(fd, 2) = quest_id;
	WFIFOSET(fd, packet_len(0x02B4));
}

//* 02b5 <packet_len>.w <mob_num>.w { <quest_id>.d <mob_id>.d <count_total>.w <count_partial>.w }.mob_num
void clif_quest_update_objective(struct map_session_data * sd, struct quest * qd, int index)
{
	int fd = sd->fd;
	int i;
	int len = quest_db[index].num_objectives*12+6;

	WFIFOHEAD(fd, len);
	WFIFOW(fd, 0) = 0x02B5;
	WFIFOW(fd, 2) = len;
	WFIFOW(fd, 4) = quest_db[index].num_objectives;

	for( i = 0; i < quest_db[index].num_objectives; i++ )
	{
		WFIFOL(fd, i*12+6) = qd->quest_id;
		WFIFOL(fd, i*12+10) = quest_db[index].mob[i];
		WFIFOW(fd, i*12+14) = quest_db[index].count[i];
		WFIFOW(fd, i*12+16) = qd->count[i];
	}

	WFIFOSET(fd, len);
}


//* 02B6 <quest_id>.L <state>.B
void clif_parse_questStateAck(int fd, struct map_session_data * sd)
{
	quest_update_status(sd, RFIFOL(fd,2), RFIFOB(fd,6)?Q_ACTIVE:Q_INACTIVE);
}

//* 02B7 <quest_id>.L <state_to>.B
void clif_quest_update_status(struct map_session_data * sd, int quest_id, bool active)
{
	int fd = sd->fd;

	WFIFOHEAD(fd, packet_len(0x02B7));
	WFIFOW(fd, 0) = 0x02B7;
	WFIFOL(fd, 2) = quest_id;
	WFIFOB(fd, 6) = active;
	WFIFOSET(fd, packet_len(0x02B7));
}

void clif_quest_show_event(struct map_session_data *sd, struct block_list *bl, short state, short color)
{
#if PACKETVER >= 20090218
	int fd = sd->fd;

	WFIFOHEAD(fd, packet_len(0x446));
	WFIFOW(fd, 0) = 0x446;
	WFIFOL(fd, 2) = bl->id;
	WFIFOW(fd, 6) = bl->x;
	WFIFOW(fd, 8) = bl->y;
	WFIFOW(fd, 10) = state;
	WFIFOW(fd, 12) = color;
	WFIFOSET(fd, packet_len(0x446));
#endif
}

/*==========================================
 * Mercenary System
 *==========================================*/
void clif_mercenary_updatestatus(struct map_session_data *sd, int type)
{
	struct mercenary_data *md;
	struct status_data *status;
	int fd;
	if( sd == NULL || (md = sd->md) == NULL )
		return;

	fd = sd->fd;
	status = &md->battle_status;
	WFIFOHEAD(fd,8);
	WFIFOW(fd,0) = 0x02a2;
	WFIFOW(fd,2) = type;
	switch( type )
	{
		case SP_ATK1:
			{
				int atk = rand()%(status->rhw.atk2 - status->rhw.atk + 1) + status->rhw.atk;
				WFIFOL(fd,4) = cap_value(atk, 0, INT16_MAX);
			}
			break;
		case SP_MATK1:
			WFIFOL(fd,4) = status_calc_matk_max(&md->bl,status);
			break;
		case SP_HIT:
			WFIFOL(fd,4) = status->hit;
			break;
		case SP_CRITICAL:
			WFIFOL(fd,4) = status->cri/10;
			break;
		case SP_DEF1:
			WFIFOL(fd,4) = status->def;
			break;
		case SP_MDEF1:
			WFIFOL(fd,4) = status->mdef;
			break;
		case SP_MERCFLEE:
			WFIFOL(fd,4) = status->flee;
			break;
		case SP_ASPD:
			WFIFOL(fd,4) = status->amotion;
			break;
		case SP_HP:
			WFIFOL(fd,4) = status->hp;
			break;
		case SP_MAXHP:
			WFIFOL(fd,4) = status->max_hp;
			break;
		case SP_SP:
			WFIFOL(fd,4) = status->sp;
			break;
		case SP_MAXSP:
			WFIFOL(fd,4) = status->max_sp;
			break;
		case SP_MERCKILLS:
			WFIFOL(fd,4) = md->mercenary.kill_count;
			break;
		case SP_MERCFAITH:
			WFIFOL(fd,4) = mercenary_get_faith(md);
			break;
	}
	WFIFOSET(fd,8);
}

void clif_mercenary_info(struct map_session_data *sd)
{
	int fd;
	struct mercenary_data *md;
	struct status_data *status;
	int temp;

	if( sd == NULL || (md = sd->md) == NULL )
		return;

	fd = sd->fd;
	status = &md->battle_status;

	WFIFOHEAD(fd,80);
	WFIFOW(fd,0) = 0x029b;
	WFIFOL(fd,2) = md->bl.id;

	// Mercenary shows ATK as a random value between ATK ~ ATK2
	temp = rand()%(status->rhw.atk2 - status->rhw.atk + 1) + status->rhw.atk;
	WFIFOW(fd,6) = cap_value(temp, 0, INT16_MAX);
	temp = status_calc_matk_max(&md->bl,status);
	WFIFOW(fd,8) = cap_value(temp, 0, INT16_MAX);
	WFIFOW(fd,10) = status->hit;
	WFIFOW(fd,12) = status->cri/10;
	WFIFOW(fd,14) = status->def;
	WFIFOW(fd,16) = status->mdef;
	WFIFOW(fd,18) = status->flee;
	WFIFOW(fd,20) = status->amotion;
	safestrncpy((char*)WFIFOP(fd,22), md->db->name, NAME_LENGTH);
	WFIFOW(fd,46) = md->db->lv;
	WFIFOL(fd,48) = status->hp;
	WFIFOL(fd,52) = status->max_hp;
	WFIFOL(fd,56) = status->sp;
	WFIFOL(fd,60) = status->max_sp;
	WFIFOL(fd,64) = (int)time(NULL) + (mercenary_get_lifetime(md) / 1000);
	WFIFOW(fd,68) = mercenary_get_faith(md);
	WFIFOL(fd,70) = mercenary_get_calls(md);
	WFIFOL(fd,74) = md->mercenary.kill_count;
	WFIFOW(fd,78) = md->battle_status.rhw.range;
	WFIFOSET(fd,80);
}

void clif_mercenary_skillblock(struct map_session_data *sd)
{
	struct mercenary_data *md;
	int fd, i, len = 4, id, j;

	if( sd == NULL || (md = sd->md) == NULL )
		return;
	
	fd = sd->fd;
	WFIFOHEAD(fd,4+37*MAX_MERCSKILL);
	WFIFOW(fd,0) = 0x029d;
	for( i = 0; i < MAX_MERCSKILL; i++ )
	{
		if( (id = md->db->skill[i].id) == 0 )
			continue;
		j = id - MC_SKILLBASE;
		WFIFOW(fd,len) = id;
		WFIFOW(fd,len+2) = skill_get_inf(id);
		WFIFOW(fd,len+4) = 0;
		WFIFOW(fd,len+6) = md->db->skill[j].lv;
		WFIFOW(fd,len+8) = skill_get_sp(id, md->db->skill[j].lv);
		WFIFOW(fd,len+10) = skill_get_range2(&md->bl, id, md->db->skill[j].lv);
		safestrncpy((char*)WFIFOP(fd,len+12), skill_get_name(id), NAME_LENGTH);
		WFIFOB(fd,len+36) = 0; // Skillable for Mercenary?
		len += 37;
	}

	WFIFOW(fd,2) = len;
	WFIFOSET(fd,len);
}

void clif_parse_mercenary_action(int fd, struct map_session_data* sd)
{
	int option = RFIFOB(fd,2);
	if( sd->md == NULL )
		return;

	if( option == 2 ) merc_delete(sd->md, 2);
}

/*------------------------------------------
 * Mercenary Message
 * 0 = Mercenary soldier's duty hour is over.
 * 1 = Your mercenary soldier has been killed.
 * 2 = Your mercenary soldier has been fired.
 * 3 = Your mercenary soldier has ran away.
 *------------------------------------------*/
void clif_mercenary_message(struct map_session_data* sd, int message)
{
	clif_msg(sd, 1266 + message);
}

/*------------------------------------------
 * Rental System Messages
 *------------------------------------------*/
void clif_rental_time(int fd, int nameid, int seconds)
{ // '<ItemName>' item will disappear in <seconds/60> minutes.
	WFIFOHEAD(fd,8);
	WFIFOW(fd,0) = 0x0298;
	WFIFOW(fd,2) = nameid;
	WFIFOL(fd,4) = seconds;
	WFIFOSET(fd,8);
}


/// Deletes a rental item from client's inventory (ZC_CASH_ITEM_DELETE).
/// 0299 <index>.W <nameid>.W
void clif_rental_expired(int fd, int index, int nameid)
{ // '<ItemName>' item has been deleted from the Inventory
	WFIFOHEAD(fd,6);
	WFIFOW(fd,0) = 0x0299;
	WFIFOW(fd,2) = index+2;
	WFIFOW(fd,4) = nameid;
	WFIFOSET(fd,6);
}

/*------------------------------------------
 * Book Reading
 *------------------------------------------*/
void clif_readbook(int fd, int book_id, int page)
{
	WFIFOHEAD(fd,10);
	WFIFOW(fd,0) = 0x0294;
	WFIFOL(fd,2) = book_id;
	WFIFOL(fd,6) = page;
	WFIFOSET(fd,10);
}

/*------------------------------------------
 * BattleGround Packets
 *------------------------------------------*/
int clif_bg_hp(struct map_session_data *sd)
{
	unsigned char buf[16];
#if PACKETVER < 20100126
	const int cmd = 0x106;
#else
	const int cmd = 0x80e;
#endif
	nullpo_ret(sd);

	WBUFW(buf,0)=cmd;
	WBUFL(buf,2) = sd->status.account_id;
#if PACKETVER < 20100126
	if( sd->battle_status.max_hp > INT16_MAX )
	{ // To correctly display the %hp bar. [Skotlex]
		WBUFW(buf,6) = sd->battle_status.hp/(sd->battle_status.max_hp/100);
		WBUFW(buf,8) = 100;
	}
	else
	{
		WBUFW(buf,6) = sd->battle_status.hp;
		WBUFW(buf,8) = sd->battle_status.max_hp;
	}
#else
	WBUFL(buf,6) = sd->battle_status.hp;
	WBUFL(buf,10) = sd->battle_status.max_hp;
#endif
	clif_send(buf, packet_len(cmd), &sd->bl, BG_AREA_WOS);
	return 0;
}

int clif_bg_xy(struct map_session_data *sd)
{
	unsigned char buf[10];
	nullpo_ret(sd);

	WBUFW(buf,0)=0x1eb;
	WBUFL(buf,2)=sd->status.account_id;
	WBUFW(buf,6)=sd->bl.x;
	WBUFW(buf,8)=sd->bl.y;
	clif_send(buf, packet_len(0x1eb), &sd->bl, BG_SAMEMAP_WOS);
	return 0;
}

int clif_bg_xy_remove(struct map_session_data *sd)
{
	unsigned char buf[10];
	nullpo_ret(sd);

	WBUFW(buf,0)=0x1eb;
	WBUFL(buf,2)=sd->status.account_id;
	WBUFW(buf,6)=-1;
	WBUFW(buf,8)=-1;
	clif_send(buf,packet_len(0x1eb),&sd->bl,BG_SAMEMAP_WOS);
	return 0;
}

void clif_bg_belonginfo(struct map_session_data *sd)
{
	int fd;
	struct guild *g;
	nullpo_retv(sd);

	if( !(battle_config.bg_eAmod_mode && sd->bg_id && (g = bg_guild_get(sd->bg_id)) != NULL) )
		return;

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len(0x16c));
	memset(WFIFOP(fd,0),0,packet_len(0x16c));
	WFIFOW(fd,0) = 0x16c;
	WFIFOL(fd,2) = g->guild_id;
	WFIFOL(fd,6) = g->emblem_id;
	WFIFOL(fd,10) = 0;
	WFIFOB(fd,14) = 0;
	WFIFOL(fd,15) = 0;
	memcpy(WFIFOP(fd,19), g->name, NAME_LENGTH);
	WFIFOSET(fd,packet_len(0x16c));
}

int clif_visual_guild_id(struct block_list *bl)
{
	struct battleground_data *bg;
	int bg_id;
	nullpo_ret(bl);

	if( battle_config.bg_eAmod_mode && (bg_id = bg_team_get_id(bl)) > 0 && (bg = bg_team_search(bg_id)) != NULL && bg->g )
		return bg->g->guild_id;
	else
		return status_get_guild_id(bl);
}

int clif_visual_emblem_id(struct block_list *bl)
{
	struct battleground_data *bg;
	int bg_id;
	nullpo_ret(bl);

	if( battle_config.bg_eAmod_mode && (bg_id = bg_team_get_id(bl)) > 0 && (bg = bg_team_search(bg_id)) != NULL && bg->g )
		return bg->g->emblem_id;
	else
		return status_get_emblem_id(bl);
}

int clif_bg_emblem(struct map_session_data *sd, struct guild *g)
{
	int fd;

	nullpo_ret(sd);
	nullpo_ret(g);

	if( g->emblem_len <= 0 )
		return 0;

	fd = sd->fd;
	WFIFOHEAD(fd,g->emblem_len+12);
	WFIFOW(fd,0)=0x152;
	WFIFOW(fd,2)=g->emblem_len+12;
	WFIFOL(fd,4)=g->guild_id;
	WFIFOL(fd,8)=g->emblem_id;
	memcpy(WFIFOP(fd,12),g->emblem_data,g->emblem_len);
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}

int clif_bg_memberlist(struct map_session_data *sd)
{
	int fd, i, c;
	struct battleground_data *bg;
	struct map_session_data *psd;
	nullpo_ret(sd);

	if( (fd = sd->fd) == 0 )
		return 0;
	if( !(battle_config.bg_eAmod_mode && sd->bg_id && (bg = bg_team_search(sd->bg_id)) != NULL) )
		return 0;

	WFIFOHEAD(fd,bg->count * 104 + 4);
	WFIFOW(fd,0) = 0x154;
	for( i = 0, c = 0; i < bg->count; i++ )
	{
		if( (psd = bg->members[i].sd) == NULL )
			continue;
		WFIFOL(fd,c*104+ 4) = psd->status.account_id;
		WFIFOL(fd,c*104+ 8) = psd->status.char_id;
		WFIFOW(fd,c*104+12) = psd->status.hair;
		WFIFOW(fd,c*104+14) = psd->status.hair_color;
		WFIFOW(fd,c*104+16) = psd->status.sex;
		WFIFOW(fd,c*104+18) = psd->status.class_;
		WFIFOW(fd,c*104+20) = psd->status.base_level;
		WFIFOL(fd,c*104+22) = psd->bg_kills; // Exp slot used to show kills
		WFIFOL(fd,c*104+26) = 1; // Online
		WFIFOL(fd,c*104+30) = psd->bmaster_flag ? 0 : 1; // Position
		if( psd->state.bg_afk )
			memcpy(WFIFOP(fd,c*104+34),"AFK",50);
		else
			memset(WFIFOP(fd,c*104+34),0,50);
		memcpy(WFIFOP(fd,c*104+84),psd->status.name,NAME_LENGTH);
		c++;
	}
	WFIFOW(fd, 2)=c*104+4;
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}

int clif_bg_leave(struct map_session_data *sd, const char *name, const char *mes)
{
	unsigned char buf[128];
	nullpo_ret(sd);

	WBUFW(buf,0)=0x15a;
	memcpy(WBUFP(buf, 2),name,NAME_LENGTH);
	memcpy(WBUFP(buf,26),mes,40);
	clif_send(buf,packet_len(0x15a),&sd->bl,BG);
	return 0;
}

int clif_bg_leave_single(struct map_session_data *sd, const char *name, const char *mes)
{
	int fd;
	nullpo_ret(sd);

	fd = sd->fd;
	WFIFOHEAD(fd,66);
	WFIFOW(fd,0) = 0x15a;
	memcpy(WFIFOP(fd,2), name, NAME_LENGTH);
	memcpy(WFIFOP(fd,26), mes, 40);
	WFIFOSET(fd,66);
	return 0;
}

/// Notifies clients of a battleground message (ZC_BATTLEFIELD_CHAT)
/// 02dc <packet len>.W <account id>.L <name>.24B <message>.?B
int clif_bg_message(struct battleground_data *bg, int src_id, const char *name, const char *mes, int len)
{
	struct map_session_data *sd;
	unsigned char *buf;
	if( (sd = bg_getavailablesd(bg)) == NULL )
		return 0;

	buf = (unsigned char*)aMallocA((len + NAME_LENGTH + 8)*sizeof(unsigned char));

	WBUFW(buf,0) = 0x2dc;
	WBUFW(buf,2) = len + NAME_LENGTH + 8;
	WBUFL(buf,4) = src_id;
	memcpy(WBUFP(buf,8), name, NAME_LENGTH);
	memcpy(WBUFP(buf,32), mes, len);
	clif_send(buf,WBUFW(buf,2), &sd->bl, BG);

	if( buf ) aFree(buf);
	return 0;
}

int clif_bg_expulsion_single(struct map_session_data *sd, const char *name, const char *mes)
{
	int fd;
	nullpo_ret(sd);

	fd = sd->fd;
	WFIFOHEAD(fd, 90);
	WFIFOW(fd,0) = 0x15c;
	safestrncpy((char*)WFIFOP(fd,2), name, NAME_LENGTH);
	safestrncpy((char*)WFIFOP(fd,26), mes, 40);
	safestrncpy((char*)WFIFOP(fd,66), "", NAME_LENGTH);
	WFIFOSET(fd,90);
	return 0;
}

/*==========================================
 * Validates and processes battlechat messages [pakpil]
 * S 0x2db <packet len>.w <text>.?B (<name> : <message>) 00
 *------------------------------------------*/
void clif_parse_BattleChat(int fd, struct map_session_data* sd)
{
	const char* text = (char*)RFIFOP(fd,4);
	int textlen = RFIFOW(fd,2) - 4;

	char *name, *message;
	int namelen, messagelen;

	if( !clif_process_message(sd, 0, &name, &namelen, &message, &messagelen) )
		return;

	if( is_atcommand(fd, sd, message, 1) )
		return;

	if( sd->sc.data[SC_BERSERK] || (sd->sc.data[SC_NOCHAT] && sd->sc.data[SC_NOCHAT]->val1&MANNER_NOCHAT) ||
		(sd->sc.data[SC_DEEPSLEEP] && sd->sc.data[SC_DEEPSLEEP]->val2) || sd->sc.data[SC_SATURDAYNIGHTFEVER])
		return;

	if( battle_config.min_chat_delay )
	{
		if( DIFF_TICK(sd->cantalk_tick, gettick()) > 0 )
			return;
		sd->cantalk_tick = gettick() + battle_config.min_chat_delay;
	}

	bg_send_message(sd, text, textlen);
}

int clif_bg_updatescore(int m)
{
	struct block_list bl;
	unsigned char buf[6];

	bl.id = 0;
	bl.type = BL_NUL;
	bl.m = m;

	WBUFW(buf,0) = 0x2de;
	WBUFW(buf,2) = map[m].bgscore_lion;
	WBUFW(buf,4) = map[m].bgscore_eagle;
	clif_send(buf,6,&bl,ALL_SAMEMAP);

	return 0;
}

int clif_bg_updatescore_single(struct map_session_data *sd)
{
	struct battleground_data *bg;
	int fd;
	nullpo_ret(sd);
	fd = sd->fd;

	if( map[sd->bl.m].flag.battleground == 2 )
	{ // Score Board on Map. Team vs Team
		WFIFOHEAD(fd,6);
		WFIFOW(fd,0) = 0x2de;
		WFIFOW(fd,2) = map[sd->bl.m].bgscore_lion;
		WFIFOW(fd,4) = map[sd->bl.m].bgscore_eagle;
		WFIFOSET(fd,6);
	}
	else if( map[sd->bl.m].flag.battleground == 3 && (bg = bg_team_search(sd->bg_id)) != NULL )
	{ // Score Board Multiple. Team vs Best Score
		WFIFOHEAD(fd,6);
		WFIFOW(fd,0) = 0x2de;
		WFIFOW(fd,2) = bg->team_score;
		WFIFOL(fd,4) = map[sd->bl.m].bgscore_top;
		WFIFOSET(fd,6);
	}

	return 0;
}

int clif_bg_updatescore_team(struct battleground_data *bg)
{
	unsigned char buf[6];
	struct map_session_data *sd;
	int i, m;

	nullpo_ret(bg);

	if( (m = map_mapindex2mapid(bg->mapindex)) < 0 )
		return 0;

	WBUFW(buf,0) = 0x2de;
	WBUFW(buf,2) = bg->team_score;
	WBUFW(buf,4) = map[m].bgscore_top;

	for( i = 0; i < MAX_BG_MEMBERS; i++ )
	{
		if( (sd = bg->members[i].sd) == NULL || sd->bl.m != m )
			continue;

		clif_send(buf,6,&sd->bl,SELF);
	}

	return 0;
}

int clif_sendbgemblem_area(struct map_session_data *sd)
{
	unsigned char buf[33];
	nullpo_ret(sd);

	WBUFW(buf, 0) = 0x2dd;
	WBUFL(buf,2) = sd->bl.id;
	safestrncpy((char*)WBUFP(buf,6), sd->status.name, NAME_LENGTH); // name don't show in screen.
	WBUFW(buf,30) = sd->bg_id;
	clif_send(buf,packet_len(0x2dd), &sd->bl, AREA);
	return 0;
}

int clif_sendbgemblem_single(int fd, struct map_session_data *sd)
{
	nullpo_ret(sd);
	WFIFOHEAD(fd,32);
	WFIFOW(fd,0) = 0x2dd;
	WFIFOL(fd,2) = sd->bl.id;
	safestrncpy((char*)WFIFOP(fd,6), sd->status.name, NAME_LENGTH);
	WFIFOW(fd,30) = sd->bg_id;
	WFIFOSET(fd,packet_len(0x2dd));
	return 0;
}

/*==========================================
 * Custom Fonts
 * S 0x2ef <account_id>.l <font id>.w
 *------------------------------------------*/
int clif_font(struct map_session_data *sd)
{
	unsigned char buf[8];
	nullpo_ret(sd);
	WBUFW(buf,0) = 0x2ef;
	WBUFL(buf,2) = sd->bl.id;
	WBUFW(buf,6) = sd->user_font;
	clif_send(buf, packet_len(0x2ef), &sd->bl, AREA);
	return 1;
}

/*==========================================
 * Instancing Window
 *------------------------------------------*/
int clif_instance(int instance_id, int type, int flag)
{
	struct map_session_data *sd;
	struct party_data *p;
	unsigned char buf[255];

	if( (p = party_search(instance[instance_id].party_id)) == NULL || (sd = party_getavailablesd(p)) == NULL )
		return 0;

	switch( type )
	{
	case 1:
		// S 0x2cb <Instance name>.61B <Standby Position>.W
		// Required to start the instancing information window on Client
		// This window re-appear each "refresh" of client automatically until type 4 is send to client.
		WBUFW(buf,0) = 0x02CB;
		memcpy(WBUFP(buf,2),instance[instance_id].name,INSTANCE_NAME_LENGTH);
		WBUFW(buf,63) = flag;
		clif_send(buf,packet_len(0x02CB),&sd->bl,PARTY);
		break;
	case 2:
		// S 0x2cc <Standby Position>.W
		// To announce Instancing queue creation if no maps available
		WBUFW(buf,0) = 0x02CC;
		WBUFW(buf,2) = flag;
		clif_send(buf,packet_len(0x02CC),&sd->bl,PARTY);
		break;
	case 3:
	case 4:
		// S 0x2cd <Instance Name>.61B <Instance Remaining Time>.L <Instance Noplayers close time>.L
		WBUFW(buf,0) = 0x02CD;
		memcpy(WBUFP(buf,2),instance[instance_id].name,61);
		if( type == 3 )
		{
			WBUFL(buf,63) = (uint32)instance[instance_id].progress_timeout;
			WBUFL(buf,67) = 0;
		}
		else
		{
			WBUFL(buf,63) = 0;
			WBUFL(buf,67) = (uint32)instance[instance_id].idle_timeout;
		}
		clif_send(buf,packet_len(0x02CD),&sd->bl,PARTY);
		break;
	case 5:
		// S 0x2ce <Message ID>.L
		// 0 = Notification (EnterLimitDate update?)
		// 1 = The Memorial Dungeon expired; it has been destroyed
		// 2 = The Memorial Dungeon's entry time limit expired; it has been destroyed
		// 3 = The Memorial Dungeon has been removed.
		// 4 = Create failure (removes the instance window)
		WBUFW(buf,0) = 0x02CE;
		WBUFL(buf,2) = flag;
		//WBUFL(buf,6) = EnterLimitDate;
		clif_send(buf,packet_len(0x02CE),&sd->bl,PARTY);
		break;
	}
	return 0;
}

void clif_instance_join(int fd, int instance_id)
{
	if( instance[instance_id].idle_timer != INVALID_TIMER )
	{
		WFIFOHEAD(fd,packet_len(0x02CD));
		WFIFOW(fd,0) = 0x02CD;
		memcpy(WFIFOP(fd,2),instance[instance_id].name,61);
		WFIFOL(fd,63) = 0;
		WFIFOL(fd,67) = (uint32)instance[instance_id].idle_timeout;
		WFIFOSET(fd,packet_len(0x02CD));
	}
	else if( instance[instance_id].progress_timer != INVALID_TIMER )
	{
		WFIFOHEAD(fd,packet_len(0x02CD));
		WFIFOW(fd,0) = 0x02CD;
		memcpy(WFIFOP(fd,2),instance[instance_id].name,61);
		WFIFOL(fd,63) = (uint32)instance[instance_id].progress_timeout;;
		WFIFOL(fd,67) = 0;
		WFIFOSET(fd,packet_len(0x02CD));
	}
	else
	{
		WFIFOHEAD(fd,packet_len(0x02CB));
		WFIFOW(fd,0) = 0x02CB;
		memcpy(WFIFOP(fd,2),instance[instance_id].name,61);
		WFIFOW(fd,63) = 0;
		WFIFOSET(fd,packet_len(0x02CB));
	}
}

void clif_instance_leave(int fd)
{
	WFIFOHEAD(fd,packet_len(0x02CE));
	WFIFOW(fd,0) = 0x02ce;
	WFIFOL(fd,2) = 4;
	WFIFOSET(fd,packet_len(0x02CE));
}

void clif_party_show_picker(struct map_session_data * sd, struct item * item_data)
{
#if PACKETVER >= 20071002
	unsigned char buf[22];
	struct item_data* id = itemdb_search(item_data->nameid);

	WBUFW(buf,0)=0x2b8;
	WBUFL(buf,2) = sd->status.account_id;
	WBUFW(buf,6) = item_data->nameid;
	WBUFB(buf,8) = item_data->identify;
	WBUFB(buf,9) = item_data->attribute;
	WBUFB(buf,10) = item_data->refine;
	clif_addcards(WBUFP(buf,11), item_data);
	WBUFW(buf,19) = id->equip; // equip location
	WBUFB(buf,21) = itemtype(id->type); // item type
	clif_send(buf, packet_len(0x2b8), &sd->bl, PARTY_SAMEMAP_WOS);
#endif
}

void clif_equip_damaged(struct map_session_data *sd, int equip_index)
{
#if PACKETVER >= 20070521
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x2bb));
	WFIFOW(fd,0) = 0x2bb;
	WFIFOW(fd,2) = equip_index;
	WFIFOL(fd,4) = sd->bl.id;
	WFIFOSET(fd,packet_len(0x2bb));
#endif
}

void clif_millenniumshield(struct map_session_data *sd, short shields )
{
#if PACKETVER >= 20081217
	unsigned char buf[10];

	WBUFW(buf,0) = 0x440;
	WBUFL(buf,2) = sd->bl.id;
	WBUFW(buf,6) = shields;
	WBUFW(buf,8) = 0;
	clif_send(buf,packet_len(0x440),&sd->bl,AREA);
#endif
}

// Display gain exp
// type = 1 -> base_exp
// type = 2 -> job_exp
// flag = 0 -> normal exp gain/lost
// flag = 1 -> quest exp gain/lost
void clif_displayexp(struct map_session_data *sd, unsigned int exp, char type, bool quest)
{
	int fd;

	nullpo_retv(sd);

	fd = sd->fd;

	WFIFOHEAD(fd, packet_len(0x7f6));
	WFIFOW(fd,0) = 0x7f6;
	WFIFOL(fd,2) = sd->bl.id;
	WFIFOL(fd,6) = exp;
	WFIFOW(fd,10) = type;
	WFIFOW(fd,12) = quest?1:0;// Normal exp is shown in yellow, quest exp is shown in purple.
	WFIFOSET(fd,packet_len(0x7f6));

	return;
}

/// Displays digital clock digits on top of the screen (ZC_SHOWDIGIT).
/// type:
///   0: Displays 'value' for 5 seconds.
///   1: Incremental counter (1 tick/second), negated 'value' specifies start value (e.g. using -10 lets the counter start at 10).
///   2: Decremental counter (1 tick/second), negated 'value' specifies start value (does not stop when reaching 0, but overflows).
///   3: Decremental counter (2 ticks/second), 'value' specifies start value (stops when reaching 0, displays at most 2 digits).
/// value:
///   Except for type 3 it is interpreted as seconds for displaying as DD:HH:MM:SS, HH:MM:SS, MM:SS or SS (leftmost '00' is not displayed).
void clif_showdigit(struct map_session_data* sd, unsigned char type, int value)
{
	WFIFOHEAD(sd->fd, packet_len(0x1b1));
	WFIFOW(sd->fd,0) = 0x1b1;
	WFIFOB(sd->fd,0) = type;
	WFIFOL(sd->fd,0) = value;
	WFIFOSET(sd->fd, packet_len(0x1b1));
}

/// S 07e4 <length>.w <option>.l <val>.l {<index>.w <amount>.w).4b*
void clif_parse_ItemListWindowSelected(int fd, struct map_session_data* sd)
{
	int n = (RFIFOW(fd,2)-12) / 4;
	int type = RFIFOL(fd,4);
	int flag = RFIFOL(fd,8); // Button clicked: 0 = Cancel, 1 = OK
	unsigned short* item_list = (unsigned short*)RFIFOP(fd,12);

	if( sd->state.trading || sd->npc_shopid )
		return;
	
	if( flag == 0 || n == 0)
	{
		sd->menuskill_id = sd->menuskill_val = sd->menuskill_itemused = 0;
		return; // Canceled by player.
	}

	if( sd->menuskill_id != SO_EL_ANALYSIS && sd->menuskill_id != GN_CHANGEMATERIAL )
	{		
		sd->menuskill_id = sd->menuskill_val = sd->menuskill_itemused = 0;
		return; // Prevent hacking.
	}

	switch( type )
	{
		case 0: // Change Material
			skill_changematerial(sd,n,item_list);
			break;
		case 1:	// Level 1: Pure to Rough
		case 2:	// Level 2: Rough to Pure
			skill_elementalanalysis(sd,n,type,item_list);
			break;
	}
	sd->menuskill_id = sd->menuskill_val = sd->menuskill_itemused = 0;
	
	return;
}

/*==========================================
 * Elemental System
 *==========================================*/
void clif_elemental_updatestatus(struct map_session_data *sd, int type)
{
	struct elemental_data *ed;
	struct status_data *status;
	int fd;
	if( sd == NULL || (ed = sd->ed) == NULL )
		return;

	fd = sd->fd;
	status = &ed->battle_status;
	WFIFOHEAD(fd,8);
	WFIFOW(fd,0) = 0x81e;
	WFIFOW(fd,2) = type;
	switch( type )
	{
		case SP_HP:
			WFIFOL(fd,4) = status->hp;
			break;
		case SP_MAXHP:
			WFIFOL(fd,4) = status->max_hp;
			break;
		case SP_SP:
			WFIFOL(fd,4) = status->sp;
			break;
		case SP_MAXSP:
			WFIFOL(fd,4) = status->max_sp;
			break;
	}
	WFIFOSET(fd,8);
}

void clif_elemental_info(struct map_session_data *sd)
{
	int fd;
	struct elemental_data *ed;
	struct status_data *status;

	if( sd == NULL || (ed = sd->ed) == NULL )
		return;

	fd = sd->fd;
	status = &ed->battle_status;

	WFIFOHEAD(fd,22);
	WFIFOW(fd, 0) = 0x81d;
	WFIFOL(fd, 2) = ed->bl.id;
	WFIFOL(fd, 6) = status->hp;
	WFIFOL(fd,10) = status->max_hp;
	WFIFOL(fd,14) = status->sp;
	WFIFOL(fd,18) = status->max_sp;
	WFIFOSET(fd,22);
}


/// Notification of the state of client command /effect (CZ_LESSEFFECT)
/// 021d <state>.L
/// state:
///     0 = Full effects
///     1 = Reduced effects
///
/// @note   The state is used on Aegis for sending skill unit packet
///         0x11f (ZC_SKILL_ENTRY) instead of 0x1c9 (ZC_SKILL_ENTRY2)
///         whenever possible. Due to the way the decision check is
///         constructed, this state tracking was rendered useless,
///         as the only skill unit, that is sent with 0x1c9 is
///         Graffiti.
void clif_parse_LessEffect(int fd, struct map_session_data* sd)
{
	int isLess = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);

	sd->state.lesseffect = ( isLess != 0 );
}


/// Buying Store System
///

/// Opens preparation window for buying store (ZC_OPEN_BUYING_STORE)
/// 0810 <slots>.B
void clif_buyingstore_open(struct map_session_data* sd)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x810));
	WFIFOW(fd,0) = 0x810;
	WFIFOB(fd,2) = sd->buyingstore.slots;
	WFIFOSET(fd,packet_len(0x810));
}


/// Request to create a buying store (CZ_REQ_OPEN_BUYING_STORE)
/// 0811 <packet len>.W <limit zeny>.L <result>.B <store name>.80B { <name id>.W <amount>.W <price>.L }*
/// result:
///     0 = cancel
///     1 = open
static void clif_parse_ReqOpenBuyingStore(int fd, struct map_session_data* sd)
{
	const unsigned int blocksize = 8;
	uint8* itemlist;
	char storename[MESSAGE_SIZE];
	unsigned char result;
	int zenylimit;
	unsigned int count, packet_len;
	struct s_packet_db* info = &packet_db[sd->packet_ver][RFIFOW(fd,0)];

	packet_len = RFIFOW(fd,info->pos[0]);

	// TODO: Make this check global for all variable length packets.
	if( packet_len < 89 )
	{// minimum packet length
		ShowError("clif_parse_ReqOpenBuyingStore: Malformed packet (expected length=%u, length=%u, account_id=%d).\n", 89, packet_len, sd->bl.id);
		return;
	}

	zenylimit = RFIFOL(fd,info->pos[1]);
	result    = RFIFOL(fd,info->pos[2]);
	safestrncpy(storename, (const char*)RFIFOP(fd,info->pos[3]), sizeof(storename));
	itemlist  = RFIFOP(fd,info->pos[4]);

	// so that buyingstore_create knows, how many elements it has access to
	packet_len-= info->pos[4];

	if( packet_len%blocksize )
	{
		ShowError("clif_parse_ReqOpenBuyingStore: Unexpected item list size %u (account_id=%d, block size=%u)\n", packet_len, sd->bl.id, blocksize);
		return;
	}
	count = packet_len/blocksize;

	buyingstore_create(sd, zenylimit, result, storename, itemlist, count);
}


/// Notification, that the requested buying store could not be created (ZC_FAILED_OPEN_BUYING_STORE_TO_BUYER)
/// 0812 <result>.W <total weight>.L
/// result:
///     1 = "Failed to open buying store." (0x6cd, MSI_BUYINGSTORE_OPEN_FAILED)
///     2 = "Total amount of then possessed items exceeds the weight limit by <weight/10-maxweight*90%>. Please re-enter." (0x6ce, MSI_BUYINGSTORE_OVERWEIGHT)
///     8 = "No sale (purchase) information available." (0x705)
/// other = nothing
void clif_buyingstore_open_failed(struct map_session_data* sd, unsigned short result, unsigned int weight)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x812));
	WFIFOW(fd,0) = 0x812;
	WFIFOW(fd,2) = result;
	WFIFOL(fd,4) = weight;
	WFIFOSET(fd,packet_len(0x812));
}


/// Notification, that the requested buying store was created (ZC_MYITEMLIST_BUYING_STORE)
/// 0813 <packet len>.W <account id>.L <limit zeny>.L { <price>.L <count>.W <type>.B <name id>.W }*
void clif_buyingstore_myitemlist(struct map_session_data* sd)
{
	int fd = sd->fd;
	unsigned int i;

	WFIFOHEAD(fd,12+sd->buyingstore.slots*9);
	WFIFOW(fd,0) = 0x813;
	WFIFOW(fd,2) = 12+sd->buyingstore.slots*9;
	WFIFOL(fd,4) = sd->bl.id;
	WFIFOL(fd,8) = sd->buyingstore.zenylimit;

	for( i = 0; i < sd->buyingstore.slots; i++ )
	{
		WFIFOL(fd,12+i*9) = sd->buyingstore.items[i].price;
		WFIFOW(fd,16+i*9) = sd->buyingstore.items[i].amount;
		WFIFOB(fd,18+i*9) = itemtype(itemdb_type(sd->buyingstore.items[i].nameid));
		WFIFOW(fd,19+i*9) = sd->buyingstore.items[i].nameid;
	}

	WFIFOSET(fd,WFIFOW(fd,2));
}


/// Notifies clients in area of a buying store (ZC_BUYING_STORE_ENTRY)
/// 0814 <account id>.L <store name>.80B
void clif_buyingstore_entry(struct map_session_data* sd)
{
	uint8 buf[86];

	WBUFW(buf,0) = 0x814;
	WBUFL(buf,2) = sd->bl.id;
	memcpy(WBUFP(buf,6), sd->message, MESSAGE_SIZE);

	clif_send(buf, packet_len(0x814), &sd->bl, AREA_WOS);
}
void clif_buyingstore_entry_single(struct map_session_data* sd, struct map_session_data* pl_sd)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x814));
	WFIFOW(fd,0) = 0x814;
	WFIFOL(fd,2) = pl_sd->bl.id;
	memcpy(WFIFOP(fd,6), pl_sd->message, MESSAGE_SIZE);
	WFIFOSET(fd,packet_len(0x814));
}


/// Request to close own buying store (CZ_REQ_CLOSE_BUYING_STORE)
/// 0815
static void clif_parse_ReqCloseBuyingStore(int fd, struct map_session_data* sd)
{
	buyingstore_close(sd);
}


/// Notifies clients in area that a buying store was closed (ZC_DISAPPEAR_BUYING_STORE_ENTRY)
/// 0816 <account id>.L
void clif_buyingstore_disappear_entry(struct map_session_data* sd)
{
	uint8 buf[6];

	WBUFW(buf,0) = 0x816;
	WBUFL(buf,2) = sd->bl.id;

	clif_send(buf, packet_len(0x816), &sd->bl, AREA_WOS);
}
void clif_buyingstore_disappear_entry_single(struct map_session_data* sd, struct map_session_data* pl_sd)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x816));
	WFIFOW(fd,0) = 0x816;
	WFIFOL(fd,2) = pl_sd->bl.id;
	WFIFOSET(fd,packet_len(0x816));
}


/// Request to open someone else's buying store (CZ_REQ_CLICK_TO_BUYING_STORE)
/// 0817 <account id>.L
static void clif_parse_ReqClickBuyingStore(int fd, struct map_session_data* sd)
{
	int account_id;

	account_id = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);

	buyingstore_open(sd, account_id);
}


/// Sends buying store item list (ZC_ACK_ITEMLIST_BUYING_STORE)
/// 0818 <packet len>.W <account id>.L <store id>.L <limit zeny>.L { <price>.L <amount>.W <type>.B <name id>.W }*
void clif_buyingstore_itemlist(struct map_session_data* sd, struct map_session_data* pl_sd)
{
	int fd = sd->fd;
	unsigned int i;

	WFIFOHEAD(fd,16+pl_sd->buyingstore.slots*9);
	WFIFOW(fd,0) = 0x818;
	WFIFOW(fd,2) = 16+pl_sd->buyingstore.slots*9;
	WFIFOL(fd,4) = pl_sd->bl.id;
	WFIFOL(fd,8) = pl_sd->buyer_id;
	WFIFOL(fd,12) = pl_sd->buyingstore.zenylimit;

	for( i = 0; i < pl_sd->buyingstore.slots; i++ )
	{
		WFIFOL(fd,16+i*9) = pl_sd->buyingstore.items[i].price;
		WFIFOW(fd,20+i*9) = pl_sd->buyingstore.items[i].amount;  // TODO: Figure out, if no longer needed items (amount == 0) are listed on official.
		WFIFOB(fd,22+i*9) = itemtype(itemdb_type(pl_sd->buyingstore.items[i].nameid));
		WFIFOW(fd,23+i*9) = pl_sd->buyingstore.items[i].nameid;
	}

	WFIFOSET(fd,WFIFOW(fd,2));
}


/// Request to sell items to a buying store (CZ_REQ_TRADE_BUYING_STORE)
/// 0819 <packet len>.W <account id>.L <store id>.L { <index>.W <name id>.W <amount>.W }*
static void clif_parse_ReqTradeBuyingStore(int fd, struct map_session_data* sd)
{
	const unsigned int blocksize = 6;
	uint8* itemlist;
	int account_id;
	unsigned int count, packet_len, buyer_id;
	struct s_packet_db* info = &packet_db[sd->packet_ver][RFIFOW(fd,0)];

	packet_len = RFIFOW(fd,info->pos[0]);

	if( packet_len < 12 )
	{// minimum packet length
		ShowError("clif_parse_ReqTradeBuyingStore: Malformed packet (expected length=%u, length=%u, account_id=%d).\n", 12, packet_len, sd->bl.id);
		return;
	}

	account_id = RFIFOL(fd,info->pos[1]);
	buyer_id   = RFIFOL(fd,info->pos[2]);
	itemlist   = RFIFOP(fd,info->pos[3]);

	// so that buyingstore_trade knows, how many elements it has access to
	packet_len-= info->pos[3];

	if( packet_len%blocksize )
	{
		ShowError("clif_parse_ReqTradeBuyingStore: Unexpected item list size %u (account_id=%d, buyer_id=%d, block size=%u)\n", packet_len, sd->bl.id, account_id, blocksize);
		return;
	}
	count = packet_len/blocksize;

	buyingstore_trade(sd, account_id, buyer_id, itemlist, count);
}


/// Notifies the buyer, that the buying store has been closed due to a post-trade condition (ZC_FAILED_TRADE_BUYING_STORE_TO_BUYER)
/// 081a <result>.W
/// result:
///     3 = "All items within the buy limit were purchased." (0x6cf, MSI_BUYINGSTORE_TRADE_OVERLIMITZENY)
///     4 = "All items were purchased." (0x6d0, MSI_BUYINGSTORE_TRADE_BUYCOMPLETE)
/// other = nothing
void clif_buyingstore_trade_failed_buyer(struct map_session_data* sd, short result)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x81a));
	WFIFOW(fd,0) = 0x81a;
	WFIFOW(fd,2) = result;
	WFIFOSET(fd,packet_len(0x81a));
}


/// Updates the zeny limit and an item in the buying store item list (ZC_UPDATE_ITEM_FROM_BUYING_STORE)
/// 081b <name id>.W <amount>.W <limit zeny>.L
void clif_buyingstore_update_item(struct map_session_data* sd, unsigned short nameid, unsigned short amount)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x81b));
	WFIFOW(fd,0) = 0x81b;
	WFIFOW(fd,2) = nameid;
	WFIFOW(fd,4) = amount;  // amount of nameid received
	WFIFOW(fd,6) = sd->buyingstore.zenylimit;
	WFIFOSET(fd,packet_len(0x81b));
}


/// Deletes item from inventory, that was sold to a buying store (ZC_ITEM_DELETE_BUYING_STORE)
/// 081c <index>.W <amount>.W <price>.L
/// message:
///     "%s (%d) were sold at %dz." (0x6d2, MSI_BUYINGSTORE_TRADE_SELLCOMPLETE)
///
/// @note   This function has to be called _instead_ of clif_delitem/clif_dropitem.
void clif_buyingstore_delete_item(struct map_session_data* sd, short index, unsigned short amount, int price)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x81c));
	WFIFOW(fd,0) = 0x81c;
	WFIFOW(fd,2) = index+2;
	WFIFOW(fd,4) = amount;
	WFIFOL(fd,6) = price;  // price per item, client calculates total Zeny by itself
	WFIFOSET(fd,packet_len(0x81c));
}


/// Notifies the seller, that a buying store trade failed (ZC_FAILED_TRADE_BUYING_STORE_TO_SELLER)
/// 0824 <result>.W <name id>.W
/// result:
///     5 = "The deal has failed." (0x39, MSI_DEAL_FAIL)
///     6 = "The trade failed, because the entered amount of item %s is higher, than the buyer is willing to buy." (0x6d3, MSI_BUYINGSTORE_TRADE_OVERCOUNT)
///     7 = "The trade failed, because the buyer is lacking required balance." (0x6d1, MSI_BUYINGSTORE_TRADE_LACKBUYERZENY)
/// other = nothing
void clif_buyingstore_trade_failed_seller(struct map_session_data* sd, short result, unsigned short nameid)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x824));
	WFIFOW(fd,0) = 0x824;
	WFIFOW(fd,2) = result;
	WFIFOW(fd,4) = nameid;
	WFIFOSET(fd,packet_len(0x824));
}


/// Search Store Info System
///

/// Request to search for stores (CZ_SEARCH_STORE_INFO)
/// 0835 <packet len>.W <type>.B <max price>.L <min price>.L <name id count>.B <card count>.B { <name id>.W }* { <card>.W }*
/// type:
///     0 = Vending
///     1 = Buying Store
///
/// @note   The client determines the item ids by specifying a name and optionally,
///         amount of card slots. If the client does not know about the item it
///         cannot be searched.
static void clif_parse_SearchStoreInfo(int fd, struct map_session_data* sd)
{
	const unsigned int blocksize = 2;
	const uint8* itemlist;
	const uint8* cardlist;
	unsigned char type;
	unsigned int min_price, max_price, packet_len, count, item_count, card_count;
	struct s_packet_db* info = &packet_db[sd->packet_ver][RFIFOW(fd,0)];

	packet_len = RFIFOW(fd,info->pos[0]);

	if( packet_len < 15 )
	{// minimum packet length
		ShowError("clif_parse_SearchStoreInfo: Malformed packet (expected length=%u, length=%u, account_id=%d).\n", 15, packet_len, sd->bl.id);
		return;
	}

	type       = RFIFOB(fd,info->pos[1]);
	max_price  = RFIFOL(fd,info->pos[2]);
	min_price  = RFIFOL(fd,info->pos[3]);
	item_count = RFIFOB(fd,info->pos[4]);
	card_count = RFIFOB(fd,info->pos[5]);
	itemlist   = RFIFOP(fd,info->pos[6]);
	cardlist   = RFIFOP(fd,info->pos[6]+blocksize*item_count);

	// check, if there is enough data for the claimed count of items
	packet_len-= info->pos[6];

	if( packet_len%blocksize )
	{
		ShowError("clif_parse_SearchStoreInfo: Unexpected item list size %u (account_id=%d, block size=%u)\n", packet_len, sd->bl.id, blocksize);
		return;
	}
	count = packet_len/blocksize;

	if( count < item_count+card_count )
	{
		ShowError("clif_parse_SearchStoreInfo: Malformed packet (expected count=%u, count=%u, account_id=%d).\n", item_count+card_count, count, sd->bl.id);
		return;
	}

	searchstore_query(sd, type, min_price, max_price, (const unsigned short*)itemlist, item_count, (const unsigned short*)cardlist, card_count);
}


/// Results for a store search request (ZC_SEARCH_STORE_INFO_ACK)
/// 0836 <packet len>.W <is first page>.B <is next page>.B <remaining uses>.B { <store id>.L <account id>.L <shop name>.80B <nameid>.W <item type>.B <price>.L <amount>.W <refine>.B <card1>.W <card2>.W <card3>.W <card4>.W }*
/// is first page:
///     0 = appends to existing results
///     1 = clears previous results before displaying this result set
/// is next page:
///     0 = no "next" label
///     1 = "next" label to retrieve more results
void clif_search_store_info_ack(struct map_session_data* sd)
{
	const unsigned int blocksize = MESSAGE_SIZE+26;
	int fd = sd->fd;
	unsigned int i, start, end;

	start = sd->searchstore.pages*SEARCHSTORE_RESULTS_PER_PAGE;
	end   = min(sd->searchstore.count, start+SEARCHSTORE_RESULTS_PER_PAGE);

	WFIFOHEAD(fd,7+(end-start)*blocksize);
	WFIFOW(fd,0) = 0x836;
	WFIFOW(fd,2) = 7+(end-start)*blocksize;
	WFIFOB(fd,4) = !sd->searchstore.pages;
	WFIFOB(fd,5) = searchstore_querynext(sd);
	WFIFOB(fd,6) = (unsigned char)min(sd->searchstore.uses, UINT8_MAX);

	for( i = start; i < end; i++ )
	{
		struct s_search_store_info_item* ssitem = &sd->searchstore.items[i];
		struct item it;

		WFIFOL(fd,i*blocksize+ 7) = ssitem->store_id;
		WFIFOL(fd,i*blocksize+11) = ssitem->account_id;
		memcpy(WFIFOP(fd,i*blocksize+15), ssitem->store_name, MESSAGE_SIZE);
		WFIFOW(fd,i*blocksize+15+MESSAGE_SIZE) = ssitem->nameid;
		WFIFOB(fd,i*blocksize+17+MESSAGE_SIZE) = itemtype(itemdb_type(ssitem->nameid));
		WFIFOL(fd,i*blocksize+18+MESSAGE_SIZE) = ssitem->price;
		WFIFOW(fd,i*blocksize+22+MESSAGE_SIZE) = ssitem->amount;
		WFIFOB(fd,i*blocksize+24+MESSAGE_SIZE) = ssitem->refine;

		// make-up an item for clif_addcards
		memset(&it, 0, sizeof(it));
		memcpy(&it.card, &ssitem->card, sizeof(it.card));
		it.nameid = ssitem->nameid;
		it.amount = ssitem->amount;

		clif_addcards(WFIFOP(fd,i*blocksize+25+MESSAGE_SIZE), &it);
	}

	WFIFOSET(fd,WFIFOW(fd,2));
}


/// Notification of failure when searching for stores (ZC_SEARCH_STORE_INFO_FAILED)
/// 0837 <reason>.B
/// reason:
///     0 = "No matching stores were found." (0x70b)
///     1 = "There are too many results. Please enter more detailed search term." (0x6f8)
///     2 = "You cannot search anymore." (0x706)
///     3 = "You cannot search yet." (0x708)
///     4 = "No sale (purchase) information available." (0x705)
void clif_search_store_info_failed(struct map_session_data* sd, unsigned char reason)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x837));
	WFIFOW(fd,0) = 0x837;
	WFIFOB(fd,2) = reason;
	WFIFOSET(fd,packet_len(0x837));
}


/// Request to display next page of results (CZ_SEARCH_STORE_INFO_NEXT_PAGE)
/// 0838
static void clif_parse_SearchStoreInfoNextPage(int fd, struct map_session_data* sd)
{
	searchstore_next(sd);
}


/// Opens the search store window (ZC_OPEN_SEARCH_STORE_INFO)
/// 083a <type>.W <remaining uses>.B
/// type:
///     0 = Search Stores
///     1 = Search Stores (Cash), asks for confirmation, when clicking a store
void clif_open_search_store_info(struct map_session_data* sd)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x83a));
	WFIFOW(fd,0) = 0x83a;
	WFIFOW(fd,2) = sd->searchstore.effect;
#if PACKETVER > 20100701
	WFIFOB(fd,4) = (unsigned char)min(sd->searchstore.uses, UINT8_MAX);
#endif
	WFIFOSET(fd,packet_len(0x83a));
}


/// Request to close the store search window (CZ_CLOSE_SEARCH_STORE_INFO)
/// 083b
static void clif_parse_CloseSearchStoreInfo(int fd, struct map_session_data* sd)
{
	searchstore_close(sd);
}


/// Request to invoke catalog effect on a store from search results (CZ_SSILIST_ITEM_CLICK)
/// 083c <account id>.L <store id>.L <nameid>.W
static void clif_parse_SearchStoreInfoListItemClick(int fd, struct map_session_data* sd)
{
	unsigned short nameid;
	int account_id, store_id;
	struct s_packet_db* info = &packet_db[sd->packet_ver][RFIFOW(fd,0)];

	account_id = RFIFOL(fd,info->pos[0]);
	store_id   = RFIFOL(fd,info->pos[1]);
	nameid     = RFIFOW(fd,info->pos[2]);

	searchstore_click(sd, account_id, store_id, nameid);
}


/// Notification of the store position on current map (ZC_SSILIST_ITEM_CLICK_ACK)
/// 083d <xPos>.W <yPos>.W
void clif_search_store_info_click_ack(struct map_session_data* sd, short x, short y)
{
	int fd = sd->fd;

	WFIFOHEAD(fd,packet_len(0x83d));
	WFIFOW(fd,0) = 0x83d;
	WFIFOW(fd,2) = x;
	WFIFOW(fd,4) = y;
	WFIFOSET(fd,packet_len(0x83d));
}


/// Parse function for packet debugging
void clif_parse_debug(int fd,struct map_session_data *sd)
{
	int cmd, packet_len;

	// clif_parse ensures, that there is at least 2 bytes of data
	cmd = RFIFOW(fd,0);

	if( sd )
	{
		packet_len = packet_db[sd->packet_ver][cmd].len;

		if( packet_len == 0 )
		{// unknown
			packet_len = RFIFOREST(fd);
		}
		else if( packet_len == -1 )
		{// variable length
			packet_len = RFIFOW(fd,2);  // clif_parse ensures, that this amount of data is already received
		}
		ShowDebug("Packet debug of 0x%04X (length %d), %s session #%d, %d/%d (AID/CID)\n", cmd, packet_len, sd->state.active ? "authed" : "unauthed", fd, sd->status.account_id, sd->status.char_id);
	}
	else
	{
		packet_len = RFIFOREST(fd);
		ShowDebug("Packet debug of 0x%04X (length %d), session #%d\n", cmd, packet_len, fd);
	}

	ShowDump(RFIFOP(fd,0), packet_len);
}

/*==========================================
 * Main client packet processing function
 *------------------------------------------*/
int clif_parse(int fd)
{
	int cmd, packet_ver, packet_len, err;
	TBL_PC* sd;
	int pnum;

	//TODO apply delays or disconnect based on packet throughput [FlavioJS]
	// Note: "click masters" can do 80+ clicks in 10 seconds

	for( pnum = 0; pnum < 3; ++pnum )// Limit max packets per cycle to 3 (delay packet spammers) [FlavioJS]  -- This actually aids packet spammers, but stuff like /str+ gets slow without it [Ai4rei]
	{ // begin main client packet processing loop

	sd = (TBL_PC *)session[fd]->session_data;
	if (session[fd]->flag.eof) {
		if (sd) {
			if (sd->state.autotrade) {
				//Disassociate character from the socket connection.
				session[fd]->session_data = NULL;
				sd->fd = 0;
				ShowInfo("%sCharacter '"CL_WHITE"%s"CL_RESET"' logged off (using @autotrade).\n", (pc_isGM(sd))?"GM ":"", sd->status.name);
			} else
			if (sd->state.active) {
				 // Player logout display [Valaris]
				ShowInfo("%sCharacter '"CL_WHITE"%s"CL_RESET"' logged off.\n", (pc_isGM(sd))?"GM ":"", sd->status.name);
				clif_quitsave(fd, sd);
			} else {
				//Unusual logout (during log on/off/map-changer procedure)
				ShowInfo("Player AID:%d/CID:%d logged off.\n", sd->status.account_id, sd->status.char_id);
				map_quit(sd);
			}
		} else {
			ShowInfo("Closed connection from '"CL_WHITE"%s"CL_RESET"'.\n", ip2str(session[fd]->client_addr, NULL));
		}
		do_close(fd);
		return 0;
	}

	if (RFIFOREST(fd) < 2)
		return 0;

	cmd = RFIFOW(fd,0);

	// identify client's packet version
	if (sd) {
		packet_ver = sd->packet_ver;
	} else {
		// check authentification packet to know packet version
		packet_ver = clif_guess_PacketVer(fd, 0, &err);
		if( err )
		{// failed to identify packet version
			ShowInfo("clif_parse: Disconnecting session #%d with unknown packet version%s.\n", fd, (
				err == 1 ? "" :
				err == 2 ? ", possibly for having an invalid account_id" :
				err == 3 ? ", possibly for having an invalid char_id." :
				/* Uncomment when checks are added in clif_guess_PacketVer. [FlavioJS]
				err == 4 ? ", possibly for having an invalid login_id1." :
				err == 5 ? ", possibly for having an invalid client_tick." :
				*/
				err == 6 ? ", possibly for having an invalid sex." :
				". ERROR invalid error code"));
			WFIFOHEAD(fd,packet_len(0x6a));
			WFIFOW(fd,0) = 0x6a;
			WFIFOB(fd,2) = 3; // Rejected from Server
			WFIFOSET(fd,packet_len(0x6a));
#ifdef DUMP_INVALID_PACKET
			ShowDump(RFIFOP(fd,0), RFIFOREST(fd));
#endif
			RFIFOSKIP(fd, RFIFOREST(fd));
			set_eof(fd);
			return 0;
		}
	}

	// filter out invalid / unsupported packets
	if (cmd > MAX_PACKET_DB || packet_db[packet_ver][cmd].len == 0) {
		ShowWarning("clif_parse: Received unsupported packet (packet 0x%04x, %d bytes received), disconnecting session #%d.\n", cmd, RFIFOREST(fd), fd);
#ifdef DUMP_INVALID_PACKET
		ShowDump(RFIFOP(fd,0), RFIFOREST(fd));
#endif
		set_eof(fd);
		return 0;
	}

	// determine real packet length
	packet_len = packet_db[packet_ver][cmd].len;
	if (packet_len == -1) { // variable-length packet
		if (RFIFOREST(fd) < 4)
			return 0;

		packet_len = RFIFOW(fd,2);
		if (packet_len < 4 || packet_len > 32768) {
			ShowWarning("clif_parse: Received packet 0x%04x specifies invalid packet_len (%d), disconnecting session #%d.\n", cmd, packet_len, fd);
#ifdef DUMP_INVALID_PACKET
			ShowDump(RFIFOP(fd,0), RFIFOREST(fd));
#endif
			set_eof(fd);
			return 0;
		}
	}
	if ((int)RFIFOREST(fd) < packet_len)
		return 0; // not enough data received to form the packet

	if( packet_db[packet_ver][cmd].func == clif_parse_debug )
		packet_db[packet_ver][cmd].func(fd, sd);
	else
	if( packet_db[packet_ver][cmd].func != NULL )
	{
		if( !sd && packet_db[packet_ver][cmd].func != clif_parse_WantToConnection )
			; //Only valid packet when there is no session
		else
		if( sd && sd->bl.prev == NULL && packet_db[packet_ver][cmd].func != clif_parse_LoadEndAck && !(cmd >= 0x6A0 && cmd <= 0x6E0) )
			; //Only valid packet when player is not on a map
		else
		if( sd && session[sd->fd]->flag.eof )
			; //No more packets accepted
		else
		if (!harm_funcs->zone_process(fd, cmd, RFIFOP(fd, 0), packet_len))
			; // Vaporized
		else
			packet_db[packet_ver][cmd].func(fd, sd); 
	}
#ifdef DUMP_UNKNOWN_PACKET
	else
	{
		const char* packet_txt = "save/packet.txt";
		FILE* fp;

		if((fp = fopen(packet_txt, "a"))!=NULL)
		{
			if( sd )
			{
				fprintf(fp, "Unknown packet 0x%04X (length %d), %s session #%d, %d/%d (AID/CID)\n", cmd, packet_len, sd->state.active ? "authed" : "unauthed", fd, sd->status.account_id, sd->status.char_id);
			}
			else
			{
				fprintf(fp, "Unknown packet 0x%04X (length %d), session #%d\n", cmd, packet_len, fd);
			}

			WriteDump(fp, RFIFOP(fd,0), packet_len);
			fprintf(fp, "\n");
			fclose(fp);
		}
		else
		{
			ShowError("Failed to write '%s'.\n", packet_txt);

			// Dump on console instead
			if( sd )
			{
				ShowDebug("Unknown packet 0x%04X (length %d), %s session #%d, %d/%d (AID/CID)\n", cmd, packet_len, sd->state.active ? "authed" : "unauthed", fd, sd->status.account_id, sd->status.char_id);
			}
			else
			{
				ShowDebug("Unknown packet 0x%04X (length %d), session #%d\n", cmd, packet_len, fd);
			}

			ShowDump(RFIFOP(fd,0), packet_len);
		}
	}
#endif

	RFIFOSKIP(fd, packet_len);

	}; // main loop end

	return 0;
}

/*==========================================
 * �p�P�b�g�f�[�^�x�[�X�ǂݍ���
 *------------------------------------------*/
static int packetdb_readdb(void)
{
	FILE *fp;
	char line[1024];
	int ln=0;
	int cmd,i,j,packet_ver;
	int max_cmd=-1;
	int skip_ver = 0;
	int warned = 0;
	char *str[64],*p,*str2[64],*p2,w1[64],w2[64];
	int packet_len_table[MAX_PACKET_DB] = {
	   10,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x0040
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
#if PACKETVER <= 20081217
	    0,  0,  0,  0, 55, 17,  3, 37, 46, -1, 23, -1,  3,110,  3,  2,
#else
	    0,  0,  0,  0, 55, 17,  3, 37, 46, -1, 23, -1,  3,114,  3,  2,
#endif
#if PACKETVER < 2
	    3, 28, 19, 11,  3, -1,  9,  5, 52, 51, 56, 58, 41,  2,  6,  6,
#elif PACKETVER < 20071106	// 78-7b �T���ȍ~ lv99�G�t�F�N�g�p
	    3, 28, 19, 11,  3, -1,  9,  5, 54, 53, 58, 60, 41,  2,  6,  6,
#elif PACKETVER <= 20081217 // change in 0x78 and 0x7c
	    3, 28, 19, 11,  3, -1,  9,  5, 55, 53, 58, 60, 42,  2,  6,  6,
#else
	    3, 28, 19, 11,  3, -1,  9,  5, 55, 53, 58, 60, 44,  2,  6,  6,
#endif
	//#0x0080
	    7,  3,  2,  2,  2,  5, 16, 12, 10,  7, 29,  2, -1, -1, -1,  0, // 0x8b changed to 2 (was 23)
	    7, 22, 28,  2,  6, 30, -1, -1,  3, -1, -1,  5,  9, 17, 17,  6,
#if PACKETVER <= 20100622
	   23,  6,  6, -1, -1, -1, -1,  8,  7,  6,  7,  4,  7,  0, -1,  6,
#else
	   23,  6,  6, -1, -1, -1, -1,  8,  7,  6,  9,  4,  7,  0, -1,  6, // 0xaa changed to 9 (was 7)
#endif
	    8,  8,  3,  3, -1,  6,  6, -1,  7,  6,  2,  5,  6, 44,  5,  3,
	//#0x00C0
	    7,  2,  6,  8,  6,  7, -1, -1, -1, -1,  3,  3,  6,  3,  2, 27, // 0xcd change to 3 (was 6)
	    3,  4,  4,  2, -1, -1,  3, -1,  6, 14,  3, -1, 28, 29, -1, -1,
	   30, 30, 26,  2,  6, 26,  3,  3,  8, 19,  5,  2,  3,  2,  2,  2,
	    3,  2,  6,  8, 21,  8,  8,  2,  2, 26,  3, -1,  6, 27, 30, 10,
	//#0x0100
	    2,  6,  6, 30, 79, 31, 10, 10, -1, -1,  4,  6,  6,  2, 11, -1,
	   10, 39,  4, 10, 31, 35, 10, 18,  2, 13, 15, 20, 68,  2,  3, 16,
	    6, 14, -1, -1, 21,  8,  8,  8,  8,  8,  2,  2,  3,  4,  2, -1,
	    6, 86,  6, -1, -1,  7, -1,  6,  3, 16,  4,  4,  4,  6, 24, 26,
	//#0x0140
	   22, 14,  6, 10, 23, 19,  6, 39,  8,  9,  6, 27, -1,  2,  6,  6,
	  110,  6, -1, -1, -1, -1, -1,  6, -1, 54, 66, 54, 90, 42,  6, 42,
	   -1, -1, -1, -1, -1, 30, -1,  3, 14,  3, 30, 10, 43, 14,186,182,
	   14, 30, 10,  3, -1,  6,106, -1,  4,  5,  4, -1,  6,  7, -1, -1,
	//#0x0180
	    6,  3,106, 10, 10, 34,  0,  6,  8,  4,  4,  4, 29, -1, 10,  6,
#if PACKETVER < 1
	   90, 86, 24,  6, 30,102,  8,  4,  8,  4, 14, 10, -1,  6,  2,  6,
#else	// 196 comodo�ȍ~ ��ԕ\���A�C�R���p
	   90, 86, 24,  6, 30,102,  9,  4,  8,  4, 14, 10, -1,  6,  2,  6,
#endif
#if PACKETVER < 20081126
	    3,  3, 35,  5, 11, 26, -1,  4,  4,  6, 10, 12,  6, -1,  4,  4,
#else // 0x1a2 changed (35->37)
	    3,  3, 37,  5, 11, 26, -1,  4,  4,  6, 10, 12,  6, -1,  4,  4,
#endif
	   11,  7, -1, 67, 12, 18,114,  6,  3,  6, 26, 26, 26, 26,  2,  3,
	//#0x01C0,   Set 0x1d5=-1
	    2, 14, 10, -1, 22, 22,  4,  2, 13, 97,  3,  9,  9, 30,  6, 28,
	    8, 14, 10, 35,  6, -1,  4, 11, 54, 53, 60,  2, -1, 47, 33,  6,
	   30,  8, 34, 14,  2,  6, 26,  2, 28, 81,  6, 10, 26,  2, -1, -1,
	   -1, -1, 20, 10, 32,  9, 34, 14,  2,  6, 48, 56, -1,  4,  5, 10,
	//#0x0200
	   26, -1, 26, 10, 18, 26, 11, 34, 14, 36, 10,  0,  0, -1, 32, 10, // 0x20c change to 0 (was 19)
	   22,  0, 26, 26, 42,  6,  6,  2,  2,282,282, 10, 10, -1, -1, 66,
#if PACKETVER < 20071106
	   10, -1, -1,  8, 10,  2,282, 18, 18, 15, 58, 57, 64,  5, 71,  5,
#else // 0x22c changed
	   10, -1, -1,  8, 10,  2,282, 18, 18, 15, 58, 57, 65,  5, 71,  5,
#endif
	   12, 26,  9, 11, -1, -1, 10,  2,282, 11,  4, 36, -1, -1,  4,  2,
	//#0x0240
	   -1, -1, -1, -1, -1,  3,  4,  8, -1,  3, 70,  4,  8, 12,  4, 10,
	    3, 32, -1,  3,  3,  5,  5,  8,  2,  3, -1,  6,  4,  6,  4,  6,
	    6,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  8,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x0280
#if PACKETVER < 20070711
	    0,  0,  0,  6,  0,  0,  0, -1,  6,  8, 18,  0,  0,  0,  0,  0,
#else
	    0,  0,  0,  6,  0,  0,  0, -1, 10, 12, 18,  0,  0,  0,  0,  0, // 0x288, 0x289 increase by 4 (kafra points)
#endif
	    0,  4,  0, 70,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	   85, -1, -1,107,  6, -1,  7,  7, 22,191,  0,  8,  0,  0,  0,  0,
	//#0x02C0
	    0,  0,  0,  0,  0, 30, 30,  0,  0,  3,  0, 65,  4, 71, 10,  0,
	   -1, -1,  0,  0, 29,  0,  6, -1, 10, 10,  3,  0, -1, 32,  6,  0,
	    0, 33,  0,  0,  0,  0,  0,  0, -1,  0, -1,  0, 67, 59, 60,  8,
	   10,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  8,  0,  0,
	//#0x0300
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x0340
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x0380
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x03C0
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x0400
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  8,  0, 25,
	//#0x0440
	   10,  4, -1,  0,  0,  0, 14,  0,  0,  0,  6,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x0480
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x04C0
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  8,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x0500
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 25,
	//#0x0540
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x0580
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x05C0
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x0600
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 25,
	//#0x0640
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x0680
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0, 22,  0, 19, 40,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x06C0
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x0700
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 25,
	//#0x0740
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x0780
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	//#0x07C0
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
#if PACKETVER < 20090617
	    6,  2, -1,  4,  4,  4,  4,  8,  8,254,  6,  8,  6, 54, 30, 54,
#else // 0x7d9 changed
	    6,  2, -1,  4,  4,  4,  4,  8,  8,268,  6,  8,  6, 54, 30, 54,
#endif
	    0, 15,  8,  6, -1,  8,  8, 32, -1,  5,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0, 14, -1, -1, -1,  8, 25,  0,  0, 26,  0,
	//#0x0800
#if PACKETVER < 20091229
 	   -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 14, 20,
#else // for Party booking ( PACKETVER >= 20091229 )
	   -1, -1, 18,  4,  8,  6,  2,  4, 14, 50, 18,  6,  2,  3, 14, 20,
#endif
	    3, -1,  8, -1, 86,  2,  6,  6, -1, -1,  4, 10, 10, 22,  8,  0,
	    0,  0,  0,  0,  6,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0, -1, -1,  3,  2, 66,  5,  2, 12,  6,  0,  0,
	//#0x0840
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0, -1, -1, -1, -1,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	};
	struct {
		void (*func)(int, struct map_session_data *);
		char *name;
	} clif_parse_func[]={
		{clif_parse_WantToConnection,"wanttoconnection"},
		{clif_parse_LoadEndAck,"loadendack"},
		{clif_parse_TickSend,"ticksend"},
		{clif_parse_WalkToXY,"walktoxy"},
		{clif_parse_QuitGame,"quitgame"},
		{clif_parse_GetCharNameRequest,"getcharnamerequest"},
		{clif_parse_GlobalMessage,"globalmessage"},
		{clif_parse_MapMove,"mapmove"},
		{clif_parse_ChangeDir,"changedir"},
		{clif_parse_Emotion,"emotion"},
		{clif_parse_HowManyConnections,"howmanyconnections"},
		{clif_parse_ActionRequest,"actionrequest"},
		{clif_parse_Restart,"restart"},
		{clif_parse_WisMessage,"wis"},
		{clif_parse_Broadcast,"broadcast"},
		{clif_parse_TakeItem,"takeitem"},
		{clif_parse_DropItem,"dropitem"},
		{clif_parse_UseItem,"useitem"},
		{clif_parse_EquipItem,"equipitem"},
		{clif_parse_UnequipItem,"unequipitem"},
		{clif_parse_NpcClicked,"npcclicked"},
		{clif_parse_NpcBuySellSelected,"npcbuysellselected"},
		{clif_parse_NpcBuyListSend,"npcbuylistsend"},
		{clif_parse_NpcSellListSend,"npcselllistsend"},
		{clif_parse_CreateChatRoom,"createchatroom"},
		{clif_parse_ChatAddMember,"chataddmember"},
		{clif_parse_ChatRoomStatusChange,"chatroomstatuschange"},
		{clif_parse_ChangeChatOwner,"changechatowner"},
		{clif_parse_KickFromChat,"kickfromchat"},
		{clif_parse_ChatLeave,"chatleave"},
		{clif_parse_TradeRequest,"traderequest"},
		{clif_parse_TradeAck,"tradeack"},
		{clif_parse_TradeAddItem,"tradeadditem"},
		{clif_parse_TradeOk,"tradeok"},
		{clif_parse_TradeCancel,"tradecancel"},
		{clif_parse_TradeCommit,"tradecommit"},
		{clif_parse_StopAttack,"stopattack"},
		{clif_parse_PutItemToCart,"putitemtocart"},
		{clif_parse_GetItemFromCart,"getitemfromcart"},
		{clif_parse_RemoveOption,"removeoption"},
		{clif_parse_ChangeCart,"changecart"},
		{clif_parse_StatusUp,"statusup"},
		{clif_parse_SkillUp,"skillup"},
		{clif_parse_UseSkillToId,"useskilltoid"},
		{clif_parse_UseSkillToPos,"useskilltopos"},
		{clif_parse_UseSkillToPosMoreInfo,"useskilltoposinfo"},
		{clif_parse_UseSkillMap,"useskillmap"},
		{clif_parse_RequestMemo,"requestmemo"},
		{clif_parse_ProduceMix,"producemix"},
		{clif_parse_Cooking,"cooking"},
		{clif_parse_NpcSelectMenu,"npcselectmenu"},
		{clif_parse_NpcNextClicked,"npcnextclicked"},
		{clif_parse_NpcAmountInput,"npcamountinput"},
		{clif_parse_NpcStringInput,"npcstringinput"},
		{clif_parse_NpcCloseClicked,"npccloseclicked"},
		{clif_parse_ItemIdentify,"itemidentify"},
		{clif_parse_SelectArrow,"selectarrow"},
		{clif_parse_AutoSpell,"autospell"},
		{clif_parse_UseCard,"usecard"},
		{clif_parse_InsertCard,"insertcard"},
		{clif_parse_RepairItem,"repairitem"},
		{clif_parse_WeaponRefine,"weaponrefine"},
		{clif_parse_SolveCharName,"solvecharname"},
		{clif_parse_ResetChar,"resetchar"},
		{clif_parse_LocalBroadcast,"localbroadcast"},
		{clif_parse_MoveToKafra,"movetokafra"},
		{clif_parse_MoveFromKafra,"movefromkafra"},
		{clif_parse_MoveToKafraFromCart,"movetokafrafromcart"},
		{clif_parse_MoveFromKafraToCart,"movefromkafratocart"},
		{clif_parse_CloseKafra,"closekafra"},
		{clif_parse_CreateParty,"createparty"},
		{clif_parse_CreateParty2,"createparty2"},
		{clif_parse_PartyInvite,"partyinvite"},
		{clif_parse_PartyInvite2,"partyinvite2"},
		{clif_parse_ReplyPartyInvite,"replypartyinvite"},
		{clif_parse_ReplyPartyInvite2,"replypartyinvite2"},
		{clif_parse_LeaveParty,"leaveparty"},
		{clif_parse_RemovePartyMember,"removepartymember"},
		{clif_parse_PartyChangeOption,"partychangeoption"},
		{clif_parse_PartyMessage,"partymessage"},
		{clif_parse_PartyChangeLeader,"partychangeleader"},
		{clif_parse_CloseVending,"closevending"},
		{clif_parse_VendingListReq,"vendinglistreq"},
		{clif_parse_PurchaseReq,"purchasereq"},
		{clif_parse_PurchaseReq2,"purchasereq2"},
		{clif_parse_OpenVending,"openvending"},
		{clif_parse_CreateGuild,"createguild"},
		{clif_parse_GuildCheckMaster,"guildcheckmaster"},
		{clif_parse_GuildRequestInfo,"guildrequestinfo"},
		{clif_parse_GuildChangePositionInfo,"guildchangepositioninfo"},
		{clif_parse_GuildChangeMemberPosition,"guildchangememberposition"},
		{clif_parse_GuildRequestEmblem,"guildrequestemblem"},
		{clif_parse_GuildChangeEmblem,"guildchangeemblem"},
		{clif_parse_GuildChangeNotice,"guildchangenotice"},
		{clif_parse_GuildInvite,"guildinvite"},
		{clif_parse_GuildReplyInvite,"guildreplyinvite"},
		{clif_parse_GuildLeave,"guildleave"},
		{clif_parse_GuildExpulsion,"guildexpulsion"},
		{clif_parse_GuildMessage,"guildmessage"},
		{clif_parse_GuildRequestAlliance,"guildrequestalliance"},
		{clif_parse_GuildReplyAlliance,"guildreplyalliance"},
		{clif_parse_GuildDelAlliance,"guilddelalliance"},
		{clif_parse_GuildOpposition,"guildopposition"},
		{clif_parse_GuildBreak,"guildbreak"},
		{clif_parse_PetMenu,"petmenu"},
		{clif_parse_CatchPet,"catchpet"},
		{clif_parse_SelectEgg,"selectegg"},
		{clif_parse_SendEmotion,"sendemotion"},
		{clif_parse_ChangePetName,"changepetname"},

		{clif_parse_GMKick,"gmkick"},
		{clif_parse_GMHide,"gmhide"},
		{clif_parse_GMReqNoChat,"gmreqnochat"},
		{clif_parse_GMReqAccountName,"gmreqaccname"},
		{clif_parse_GMKickAll,"killall"},
		{clif_parse_GMRecall,"recall"},
		{clif_parse_GMRecall,"summon"},
		{clif_parse_GM_Monster_Item,"itemmonster"},
		{clif_parse_GMShift,"remove"},
		{clif_parse_GMShift,"shift"},
		{clif_parse_GMChangeMapType,"changemaptype"},
		{clif_parse_GMRc,"rc"},
		{clif_parse_GMRecall2,"recall2"},
		{clif_parse_GMRemove2,"remove2"},

		{clif_parse_NoviceDoriDori,"sndoridori"},
		{clif_parse_NoviceExplosionSpirits,"snexplosionspirits"},
		{clif_parse_PMIgnore,"wisexin"},
		{clif_parse_PMIgnoreList,"wisexlist"},
		{clif_parse_PMIgnoreAll,"wisall"},
		{clif_parse_FriendsListAdd,"friendslistadd"},
		{clif_parse_FriendsListRemove,"friendslistremove"},
		{clif_parse_FriendsListReply,"friendslistreply"},
		{clif_parse_Blacksmith,"blacksmith"},
		{clif_parse_Alchemist,"alchemist"},
		{clif_parse_Taekwon,"taekwon"},
		{clif_parse_RankingPk,"rankingpk"},
		{clif_parse_FeelSaveOk,"feelsaveok"},
		{clif_parse_debug,"debug"},
		{clif_parse_ChangeHomunculusName,"changehomunculusname"},
		{clif_parse_HomMoveToMaster,"hommovetomaster"},
		{clif_parse_HomMoveTo,"hommoveto"},
		{clif_parse_HomAttack,"homattack"},
		{clif_parse_HomMenu,"hommenu"},
		{clif_parse_Hotkey,"hotkey"},
		{clif_parse_AutoRevive,"autorevive"},
		{clif_parse_Check,"check"},
		{clif_parse_Adopt_request,"adoptrequest"},
		{clif_parse_Adopt_reply,"adoptreply"},
#ifndef TXT_ONLY
		// MAIL SYSTEM
		{clif_parse_Mail_refreshinbox,"mailrefresh"},
		{clif_parse_Mail_read,"mailread"},
		{clif_parse_Mail_getattach,"mailgetattach"},
		{clif_parse_Mail_delete,"maildelete"},
		{clif_parse_Mail_return,"mailreturn"},
		{clif_parse_Mail_setattach,"mailsetattach"},
		{clif_parse_Mail_winopen,"mailwinopen"},
		{clif_parse_Mail_send,"mailsend"},
		// AUCTION SYSTEM
		{clif_parse_Auction_search,"auctionsearch"},
		{clif_parse_Auction_buysell,"auctionbuysell"},
		{clif_parse_Auction_setitem,"auctionsetitem"},
		{clif_parse_Auction_cancelreg,"auctioncancelreg"},
		{clif_parse_Auction_register,"auctionregister"},
		{clif_parse_Auction_cancel,"auctioncancel"},
		{clif_parse_Auction_close,"auctionclose"},
		{clif_parse_Auction_bid,"auctionbid"},
		// Quest Log System
		{clif_parse_questStateAck,"queststate"},
#endif
		{clif_parse_cashshop_buy,"cashshopbuy"},
		{clif_parse_ViewPlayerEquip,"viewplayerequip"},
		{clif_parse_EquipTick,"equiptickbox"},
		{clif_parse_BattleChat,"battlechat"},
		{clif_parse_mercenary_action,"mermenu"},
		{clif_parse_progressbar,"progressbar"},
		{clif_parse_SkillSelectMenu,"skillselectmenu"},
		{clif_parse_ItemListWindowSelected,"itemlistwindowselected"},
#if PACKETVER >= 20091229
		{clif_parse_PartyBookingRegisterReq,"bookingregreq"},
		{clif_parse_PartyBookingSearchReq,"bookingsearchreq"},
		{clif_parse_PartyBookingUpdateReq,"bookingupdatereq"},
		{clif_parse_PartyBookingDeleteReq,"bookingdelreq"},
#endif
		{clif_parse_PVPInfo,"pvpinfo"},
		{clif_parse_LessEffect,"lesseffect"},
		// Buying Store
		{clif_parse_ReqOpenBuyingStore,"reqopenbuyingstore"},
		{clif_parse_ReqCloseBuyingStore,"reqclosebuyingstore"},
		{clif_parse_ReqClickBuyingStore,"reqclickbuyingstore"},
		{clif_parse_ReqTradeBuyingStore,"reqtradebuyingstore"},
		// Store Search
		{clif_parse_SearchStoreInfo,"searchstoreinfo"},
		{clif_parse_SearchStoreInfoNextPage,"searchstoreinfonextpage"},
		{clif_parse_CloseSearchStoreInfo,"closesearchstoreinfo"},
		{clif_parse_SearchStoreInfoListItemClick,"searchstoreinfolistitemclick"},
		{NULL,NULL}
	};

	// initialize packet_db[SERVER] from hardcoded packet_len_table[] values
	memset(packet_db,0,sizeof(packet_db));
	for( i = 0; i < ARRAYLENGTH(packet_len_table); ++i )
		packet_len(i) = packet_len_table[i];

	sprintf(line, "%s/packet_db.txt", db_path);
	if( (fp=fopen(line,"r"))==NULL ){
		ShowFatalError("can't read %s\n", line);
		exit(EXIT_FAILURE);
	}

	clif_config.packet_db_ver = MAX_PACKET_VER;
	packet_ver = MAX_PACKET_VER;	// read into packet_db's version by default
#include "harmony_packets.inc"
	while( fgets(line, sizeof(line), fp) )
	{
		ln++;
		if(line[0]=='/' && line[1]=='/')
			continue;
		if (sscanf(line,"%256[^:]: %256[^\r\n]",w1,w2) == 2)
		{
			if(strcmpi(w1,"packet_ver")==0) {
				int prev_ver = packet_ver;
				skip_ver = 0;
				packet_ver = atoi(w2);
				if ( packet_ver > MAX_PACKET_VER )
				{	//Check to avoid overflowing. [Skotlex]
					if( (warned&1) == 0 )
						ShowWarning("The packet_db table only has support up to version %d.\n", MAX_PACKET_VER);
					warned &= 1;
					skip_ver = 1;
				}
				else if( packet_ver < 0 )
				{
					if( (warned&2) == 0 )
						ShowWarning("Negative packet versions are not supported.\n");
					warned &= 2;
					skip_ver = 1;
				}
				else if( packet_ver == SERVER )
				{
					if( (warned&4) == 0 )
						ShowWarning("Packet version %d is reserved for server use only.\n", SERVER);
					warned &= 4;
					skip_ver = 1;
				}

				if( skip_ver )
				{
					ShowWarning("Skipping packet version %d.\n", packet_ver);
					packet_ver = prev_ver;
					continue;
				}
				// copy from previous version into new version and continue
				// - indicating all following packets should be read into the newer version
				memcpy(&packet_db[packet_ver], &packet_db[prev_ver], sizeof(packet_db[0]));
				continue;
			} else if(strcmpi(w1,"packet_db_ver")==0) {
				if(strcmpi(w2,"default")==0) //This is the preferred version.
					clif_config.packet_db_ver = MAX_PACKET_VER;
				else // to manually set the packet DB version
					clif_config.packet_db_ver = cap_value(atoi(w2), 0, MAX_PACKET_VER);
				
				continue;
			}
		}

		if( skip_ver != 0 )
			continue; // Skipping current packet version

		memset(str,0,sizeof(str));
		for(j=0,p=line;j<4 && p; ++j)
		{
			str[j]=p;
			p=strchr(p,',');
			if(p) *p++=0;
		}
		if(str[0]==NULL)
			continue;
		cmd=strtol(str[0],(char **)NULL,0);
		if(max_cmd < cmd)
			max_cmd = cmd;
		if(cmd <= 0 || cmd > MAX_PACKET_DB)
			continue;
		if(str[1]==NULL){
			ShowError("packet_db: packet len error\n");
			continue;
		}

		packet_db[packet_ver][cmd].len = (short)atoi(str[1]);

		if(str[2]==NULL){
			packet_db[packet_ver][cmd].func = NULL;
			ln++;
			continue;
		}

		// look up processing function by name
		ARR_FIND( 0, ARRAYLENGTH(clif_parse_func), j, clif_parse_func[j].name != NULL && strcmp(str[2],clif_parse_func[j].name)==0 );
		if( j < ARRAYLENGTH(clif_parse_func) )
			packet_db[packet_ver][cmd].func = clif_parse_func[j].func;

		// set the identifying cmd for the packet_db version
		if (strcmp(str[2],"wanttoconnection")==0)
			clif_config.connect_cmd[packet_ver] = cmd;
			
		if(str[3]==NULL){
			ShowError("packet_db: packet error\n");
			exit(EXIT_FAILURE);
		}
		for(j=0,p2=str[3];p2;j++){
			short k;
			str2[j]=p2;
			p2=strchr(p2,':');
			if(p2) *p2++=0;
			k = atoi(str2[j]);
			// if (packet_db[packet_ver][cmd].pos[j] != k && clif_config.prefer_packet_db)	// not used for now
			packet_db[packet_ver][cmd].pos[j] = k;
		}
	}
	fclose(fp);
	if(max_cmd > MAX_PACKET_DB)
	{
		ShowWarning("Found packets up to 0x%X, ignored 0x%X and above.\n", max_cmd, MAX_PACKET_DB);
		ShowWarning("Please increase MAX_PACKET_DB and recompile.\n");
	}
	if (!clif_config.connect_cmd[clif_config.packet_db_ver])
	{	//Locate the nearest version that we still support. [Skotlex]
		for(j = clif_config.packet_db_ver; j >= 0 && !clif_config.connect_cmd[j]; j--);
		
		clif_config.packet_db_ver = j?j:MAX_PACKET_VER;
	}
	ShowStatus("Done reading packet database from '"CL_WHITE"%s"CL_RESET"'. Using default packet version: "CL_WHITE"%d"CL_RESET".\n", "packet_db.txt", clif_config.packet_db_ver);
	return 0;
}

/*==========================================
 *
 *------------------------------------------*/
int do_init_clif(void)
{
	clif_config.packet_db_ver = -1; // the main packet version of the DB
	memset(clif_config.connect_cmd, 0, sizeof(clif_config.connect_cmd)); //The default connect command will be determined after reading the packet_db [Skotlex]

	memset(packet_db,0,sizeof(packet_db));
	//Using the packet_db file is the only way to set up packets now [Skotlex]
	packetdb_readdb();

	set_defaultparse(clif_parse);
	if( make_listen_bind(bind_ip,map_port) == -1 )
	{
		ShowFatalError("can't bind game port\n");
		exit(EXIT_FAILURE);
	}

	add_timer_func_list(clif_clearunit_delayed_sub, "clif_clearunit_delayed_sub");
	add_timer_func_list(clif_delayquit, "clif_delayquit");
	return 0;
}