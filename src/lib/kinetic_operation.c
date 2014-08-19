/*
* kinetic-c
* Copyright (C) 2014 Seagate Technology.
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
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#include "kinetic_operation.h"
#include "kinetic_connection.h"

void KineticOperation_BuildNoop(KineticOperation* operation)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->response != NULL);

    operation->request->message->header.messagetype = KINETIC_PROTO_MESSAGE_TYPE_NOOP;
    operation->request->message->header.has_messagetype = true;
}

void KineticOperation_BuildPut(KineticOperation* operation, uint8_t* value, int64_t valueLength)
{
    assert(operation != NULL);
    assert(operation->connection != NULL);
    assert(operation->request != NULL);
    assert(operation->response != NULL);

    operation->request->message->header.messagetype = KINETIC_PROTO_MESSAGE_TYPE_PUT;
    operation->request->message->header.has_messagetype = true;

    operation->request->value = value;
    operation->request->valueLength = valueLength;
}
