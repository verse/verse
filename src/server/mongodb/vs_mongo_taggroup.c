/*
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Contributor(s): Jiri Hnidek <jiri.hnidek@tul.cz>.
 *
 */

#define MONGO_HAVE_STDINT 1

#include <mongo.h>

#include "vs_main.h"
#include "vs_mongo_main.h"
#include "vs_mongo_taggroup.h"
#include "vs_node.h"
#include "vs_taggroup.h"

#include "v_common.h"

/**
 * \brief This function saves tag group to the mongo database
 *
 * \param[in] *vs_ctx	The verse server context
 * \param[in] *node		The node containing tag group
 * \param[in] *tg		The tag group that will be saved
 *
 * \return	This function returns 1 on success. Otherwise it returns 0;
 */
int vs_mongo_taggroup_save(struct VS_CTX *vs_ctx,
		struct VSNode *node,
		struct VSTagGroup *tg)
{
	(void)vs_ctx;
	(void)node;
	(void)tg;

	return 1;
}

/**
 * \brief This function tries to load tag group from mongo database
 *
 * \param[in] *vs_ctx		The verse server context
 * \param[in] *node			The node containing tag group
 * \param[in] taggroup_id	The tag group ID that is requested from database
 * \param[in] version		The version of tag group id that is requested
 * 							from database
 * \return This function returns pointer at tag group, when tag group is found.
 * Otherwise it returns NULL.
 */
struct VSTagGroup *vs_mongo_taggroup_load(struct VS_CTX *vs_ctx,
		struct VSNode *node,
		uint16 taggroup_id,
		uint32 version)
{
	(void)vs_ctx;
	(void)node;
	(void)taggroup_id;
	(void)version;

	return NULL;
}
