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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "unity.h"
#include "unity_helper.h"
#include "kinetic_session.h"
#include "kinetic_proto.h"
#include "protobuf-c/protobuf-c.h"
#include "kinetic_logger.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_socket.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_allocator.h"
#include "mock_kinetic_client.h"
#include "mock_kinetic_pdu_unpack.h"
#include "mock_kinetic_countingsemaphore.h"
#include "mock_kinetic_resourcewaiter.h"

#include "mock_bus.h"
#include "byte_array.h"
#include <string.h>
#include <sys/time.h>

static KineticConnection Connection;
static KineticCountingSemaphore Semaphore;
static KineticSession Session;
static KineticRequest Request;
static int OperationCompleteCallbackCount;
static KineticStatus LastStatus;
static struct _KineticClient Client;
static struct bus MessageBus;

void setUp(void)
{
    memset(&Session, 0, sizeof(Session));
    Session.config = (KineticSessionConfig) {
        .host = "somehost.com",
        .port = 17,
        .clusterVersion = 6,
    };
    Client.bus = &MessageBus;
    KineticConnection_Init(&Connection);
    Connection.pSession = &Session;
    Client.bus = &MessageBus;
    KineticAllocator_NewConnection_ExpectAndReturn(&MessageBus, &Session, &Connection);
    KineticCountingSemaphore_Create_ExpectAndReturn(KINETIC_MAX_OUTSTANDING_OPERATIONS_PER_SESSION, &Semaphore);
    
    KineticStatus status = KineticSession_Create(&Session, &Client);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(&Connection, Session.connection);
    TEST_ASSERT_FALSE(Session.connection->connected);
    TEST_ASSERT_EQUAL_STRING(Session.config.host, "somehost.com");
    TEST_ASSERT_EQUAL(17, Session.config.port);

    KineticRequest_Init(&Request, &Session);
    OperationCompleteCallbackCount = 0;
    LastStatus = KINETIC_STATUS_INVALID;
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticSession_Create_should_return_KINETIC_STATUS_SESSION_EMPTY_upon_NULL_session(void)
{
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SESSION_EMPTY, KineticSession_Create(NULL, NULL));
}

void test_KineticSession_Create_should_return_KINETIC_STATUS_SESSION_EMPTY_upon_NULL_client(void)
{
    KineticSession session;
    memset(&session, 0, sizeof(session));
    session.connection = &Connection;
    TEST_ASSERT_EQUAL(KINETIC_STATUS_SESSION_EMPTY, KineticSession_Create(&session, NULL));
}

void test_KineticSession_Create_should_allocate_and_destroy_KineticConnections(void)
{
    KineticSession session;
    memset(&session, 0, sizeof(session));
    KineticConnection connection;
    memset(&connection, 0, sizeof(connection));
    connection.pSession = &session;

    KineticAllocator_NewConnection_ExpectAndReturn(&MessageBus, &session, &connection);
    KineticCountingSemaphore_Create_ExpectAndReturn(KINETIC_MAX_OUTSTANDING_OPERATIONS_PER_SESSION, &Semaphore);
    KineticStatus status = KineticSession_Create(&session, &Client);    
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL_PTR(&connection, session.connection);
    TEST_ASSERT_FALSE(session.connection->connected);

    KineticCountingSemaphore_Destroy_Expect(&Semaphore);
    KineticAllocator_FreeConnection_Expect(&connection);
    KineticAllocator_FreeSession_Expect(&session);
    status = KineticSession_Destroy(&session);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_NULL(session.connection);
}

void test_KineticConnection_Init_should_create_a_default_connection_object(void)
{
    KineticConnection connection;
    KineticConnection_Init(&connection);

    TEST_ASSERT_FALSE(connection.connected);
    TEST_ASSERT_EQUAL(-1, connection.socket);
    TEST_ASSERT_EQUAL_INT64(0, connection.sequence);
    TEST_ASSERT_EQUAL_INT64(0, connection.connectionID);
}

void test_KineticSession_Connect_should_return_KINETIC_SESSION_EMPTY_upon_NULL_session(void)
{
    KineticStatus status = KineticSession_Connect(NULL);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_SESSION_EMPTY, status);
}


void test_KineticSession_Connect_should_return_KINETIC_STATUS_CONNECTION_ERROR_upon_NULL_connection(void)
{
    KineticSession session = {.connection = NULL};

    KineticStatus status = KineticSession_Connect(&session);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);
}

void test_KineticSession_Connect_should_report_a_failed_connection(void)
{
    TEST_ASSERT_EQUAL_STRING(Session.config.host, "somehost.com");
    TEST_ASSERT_EQUAL(17, Session.config.port);

    KineticSocket_Connect_ExpectAndReturn("somehost.com", 17, KINETIC_SOCKET_DESCRIPTOR_INVALID);

    KineticStatus status = KineticSession_Connect(&Session);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);
    TEST_ASSERT_FALSE(Session.connection->connected);
    TEST_ASSERT_EQUAL(KINETIC_SOCKET_DESCRIPTOR_INVALID, Session.connection->socket);
}

void test_KineticSession_Connect_should_report_a_failure_to_receive_register_with_client(void)
{
    const uint8_t hmacKey[] = {1, 6, 3, 5, 4, 8, 19};

    KineticConnection expectedConnection = {
        .connected = true,
        .socket = 24,
    };

    KineticSession expected = {
        .config = (KineticSessionConfig) {
            .host = "valid-host.com",
            .port = 1234,
            .clusterVersion = 17,
            .identity = 12,
            .hmacKey = {
                .data = expected.config.keyData,
                .len = sizeof(hmacKey)},
        },
        .connection = &expectedConnection,
    };
    memcpy(expected.config.hmacKey.data,
        hmacKey, expected.config.hmacKey.len);

    KineticConnection actualConnection = {
        .connected = true,
        .socket = expectedConnection.socket,
        .connectionID = 5,
    };

    KineticSession session = {
        .config = (KineticSessionConfig) {
            .host = "valid-host.com",
            .port = expected.config.port,
            .clusterVersion = expected.config.clusterVersion,
            .identity = expected.config.identity,
            .hmacKey = {
                .data = Session.config.keyData,
                .len = sizeof(hmacKey)},
        },
        .connection = &actualConnection,
    };
    memcpy(session.config.hmacKey.data,
        hmacKey, expected.config.hmacKey.len);

    KineticSocket_Connect_ExpectAndReturn(expected.config.host, expected.config.port, expected.connection->socket);
    bus_register_socket_ExpectAndReturn(NULL, BUS_SOCKET_PLAIN,
        expectedConnection.socket, &actualConnection, false);
    KineticSocket_Close_Expect(expectedConnection.socket);

    KineticStatus status = KineticSession_Connect(&session);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);
    TEST_ASSERT_FALSE(session.connection->connected);
    TEST_ASSERT_EQUAL(KINETIC_SOCKET_DESCRIPTOR_INVALID, session.connection->socket);
    TEST_ASSERT_NULL(session.connection->si);
}

void test_KineticSession_Connect_should_report_a_failure_to_receive_initialization_info_from_device(void)
{
    const uint8_t hmacKey[] = {1, 6, 3, 5, 4, 8, 19};

    KineticConnection expectedConnection = {
        .connected = true,
        .socket = 24,
    };

    KineticSession expected = {
        .config = (KineticSessionConfig) {
            .host = "valid-host.com",
            .port = 1234,
            .clusterVersion = 17,
            .identity = 12,
            .hmacKey = {
                .data = expected.config.keyData,
                .len = sizeof(hmacKey)},
        },
        .connection = &expectedConnection,
    };
    memcpy(expected.config.hmacKey.data,
        hmacKey, expected.config.hmacKey.len);

    KineticConnection actualConnection = {
        .connected = true,
        .socket = expectedConnection.socket,
        .connectionID = 5,
    };

    KineticSession session = {
        .config = (KineticSessionConfig) {
            .host = "valid-host.com",
            .port = expected.config.port,
            .clusterVersion = expected.config.clusterVersion,
            .identity = expected.config.identity,
            .hmacKey = {
                .data = Session.config.keyData,
                .len = sizeof(hmacKey)},
        },
        .connection = &actualConnection,
    };
    memcpy(session.config.hmacKey.data,
        hmacKey, expected.config.hmacKey.len);

    KineticSocket_Connect_ExpectAndReturn(expected.config.host, expected.config.port, expected.connection->socket);
    bus_register_socket_ExpectAndReturn(NULL, BUS_SOCKET_PLAIN,
        expectedConnection.socket, &actualConnection, true);
    KineticResourceWaiter_WaitTilAvailable_ExpectAndReturn(&actualConnection.connectionReady,
        KINETIC_CONNECTION_TIMEOUT_SECS, false);
    KineticSocket_Close_Expect(expectedConnection.socket);

    KineticStatus status = KineticSession_Connect(&session);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_CONNECTION_ERROR, status);
    TEST_ASSERT_FALSE(session.connection->connected);
    TEST_ASSERT_EQUAL(KINETIC_SOCKET_DESCRIPTOR_INVALID, session.connection->socket);
    TEST_ASSERT_NULL(session.connection->si);
}

void test_KineticSession_Connect_should_connect_to_specified_host(void)
{
    const uint8_t hmacKey[] = {1, 6, 3, 5, 4, 8, 19};

    KineticConnection expectedConnection = {
        .connected = true,
        .socket = 24,
    };

    KineticSession expected = {
        .config = (KineticSessionConfig) {
            .host = "valid-host.com",
            .port = 1234,
            .clusterVersion = 17,
            .identity = 12,
            .hmacKey = {
                .data = expected.config.keyData,
                .len = sizeof(hmacKey)},
        },
        .connection = &expectedConnection,
    };
    memcpy(expected.config.hmacKey.data,
        hmacKey, expected.config.hmacKey.len);

    KineticConnection actualConnection = {
        .connected = false,
        .socket = expectedConnection.socket,
        .connectionID = 5,
    };

    KineticSession session = {
        .config = (KineticSessionConfig) {
            .host = "valid-host.com",
            .port = expected.config.port,
            .clusterVersion = expected.config.clusterVersion,
            .identity = expected.config.identity,
            .hmacKey = {
                .data = Session.config.keyData,
                .len = sizeof(hmacKey)},
        },
        .connection = &actualConnection,
    };
    memcpy(session.config.hmacKey.data,
        hmacKey, expected.config.hmacKey.len);

    KineticSocket_Connect_ExpectAndReturn(expected.config.host, expected.config.port, expected.connection->socket);
    bus_register_socket_ExpectAndReturn(NULL, BUS_SOCKET_PLAIN,
        expectedConnection.socket, &actualConnection, true);
    KineticResourceWaiter_WaitTilAvailable_ExpectAndReturn(&actualConnection.connectionReady,
        KINETIC_CONNECTION_TIMEOUT_SECS, true);

    // Establish connection
    KineticStatus status = KineticSession_Connect(&session);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_TRUE(session.connection->connected);
    TEST_ASSERT_EQUAL(expected.connection->socket, actualConnection.socket);
    TEST_ASSERT_EQUAL_STRING(expected.config.host, session.config.host);
    TEST_ASSERT_EQUAL(expected.config.port, session.config.port);
    TEST_ASSERT_EQUAL_INT64(expected.config.clusterVersion, session.config.clusterVersion);
    TEST_ASSERT_EQUAL_INT64(expected.config.identity, session.config.identity);
    TEST_ASSERT_EQUAL_ByteArray(expected.config.hmacKey, session.config.hmacKey);
}
