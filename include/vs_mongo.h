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

#ifndef VS_MONGO_H_
#define VS_MONGO_H_

#define MONGO_HAVE_STDINT 1

struct VS_CTX;

int vs_mongo_node_save(struct VS_CTX *vs_ctx, struct VSNode *node);

int vs_mongo_conn_init(struct VS_CTX *vs_ctx);
void vs_mongo_conn_destroy(struct VS_CTX *vs_ctx);

#endif /* VS_MONGO_H_ */
