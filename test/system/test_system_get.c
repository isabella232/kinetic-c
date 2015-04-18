/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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
#include "system_test_fixture.h"
#include "kinetic_client.h"
#include "system_test_kv_generate.h"

static const unsigned int TOTAL_PUT_KEYS = 100;
static const int DEFAULT_SIZE = 1024;

static ByteBuffer ExpectedTagBuffer;
static ByteBuffer ExpectedVersionBuffer;

void setUp(void)
{
    SystemTestSetup(1, true);

    uint8_t version_data[DEFAULT_SIZE], tag_data[DEFAULT_SIZE];
    ByteBuffer version_buffer, tag_buffer;
    version_buffer = ByteBuffer_CreateAndAppendCString(version_data, sizeof(version_data), "v1.0");
    ExpectedVersionBuffer = ByteBuffer_CreateAndAppendCString(version_data, sizeof(version_data), "v1.0");
    tag_buffer = ByteBuffer_CreateAndAppendCString(tag_data, sizeof(tag_data), "SomeTagValue");
    ExpectedTagBuffer = ByteBuffer_CreateAndAppendCString(tag_data, sizeof(tag_data), "SomeTagValue");

    unsigned int i;
    for (i=0; i<TOTAL_PUT_KEYS; i++)
    {
        KineticEntry putEntry = {
   	       .key = generate_entry_key_by_index(i),
   	       .tag = tag_buffer,
   	       .newVersion = version_buffer,
           .algorithm = KINETIC_ALGORITHM_SHA1,
           .value = generate_entry_value_by_index(i),
           .force = true,
           .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
        };

        KineticStatus status = KineticClient_Put(Fixture.session, &putEntry, NULL);
        TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    }
}

void tearDown(void)
{
    SystemTestShutDown();
}

void test_Get_should_retrieve_objects_and_metadata_from_device(void)
{
	unsigned int i;

	for (i=0; i<TOTAL_PUT_KEYS; i++)
	{
		ByteBuffer version_buffer, tag_buffer, value_buffer;
		uint8_t version_data[DEFAULT_SIZE], tag_data[DEFAULT_SIZE], value_data[DEFAULT_SIZE];
		version_buffer = ByteBuffer_Create(version_data, DEFAULT_SIZE, 0);
		tag_buffer = ByteBuffer_Create(tag_data, DEFAULT_SIZE, 0);
		value_buffer = ByteBuffer_Create(value_data, DEFAULT_SIZE, 0);
		KineticEntry getEntry = {
	        .key = generate_entry_key_by_index(i),
			.dbVersion = version_buffer,
	        .tag = tag_buffer,
			.value = value_buffer,
	    };

	    KineticStatus status = KineticClient_Get(Fixture.session, &getEntry, NULL);

	    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
	    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedVersionBuffer, getEntry.dbVersion);
	    TEST_ASSERT_ByteBuffer_NULL(getEntry.newVersion);
	    TEST_ASSERT_EQUAL_ByteBuffer(generate_entry_key_by_index(i), getEntry.key);
	    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedTagBuffer, getEntry.tag);
	    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry.algorithm);
	    TEST_ASSERT_EQUAL_ByteBuffer(generate_entry_value_by_index(i), getEntry.value);
	}
}

void test_Get_should_be_able_to_retrieve_just_metadata_from_device(void)
{
	unsigned int i;

	for (i=0; i<TOTAL_PUT_KEYS; i++)
	{
		ByteBuffer version_buffer, tag_buffer;
		uint8_t version_data[DEFAULT_SIZE], tag_data[DEFAULT_SIZE];
		version_buffer = ByteBuffer_Create(version_data, DEFAULT_SIZE, 0);
		tag_buffer = ByteBuffer_Create(tag_data, DEFAULT_SIZE, 0);
		KineticEntry getEntry = {
	        .key = generate_entry_key_by_index(i),
			.dbVersion = version_buffer,
	        .tag = tag_buffer,
			.metadataOnly = true,
	    };

		KineticStatus status = KineticClient_Get(Fixture.session, &getEntry, NULL);

	    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
	    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedVersionBuffer, getEntry.dbVersion);
	    TEST_ASSERT_ByteBuffer_NULL(getEntry.newVersion);
	    TEST_ASSERT_ByteBuffer_NULL(getEntry.value);
	    TEST_ASSERT_EQUAL_ByteBuffer(generate_entry_key_by_index(i), getEntry.key);
	    TEST_ASSERT_EQUAL_ByteBuffer(ExpectedTagBuffer, getEntry.tag);
	    TEST_ASSERT_EQUAL(KINETIC_ALGORITHM_SHA1, getEntry.algorithm);
	}
}

void test_Get_should_returns_null_for_nonexisting_key(void)
{
	uint8_t non_existing_key_data[1024];
	ByteBuffer version_buffer, tag_buffer;
	uint8_t version_data[DEFAULT_SIZE], tag_data[DEFAULT_SIZE];
	version_buffer = ByteBuffer_Create(version_data, DEFAULT_SIZE, 0);
	tag_buffer = ByteBuffer_Create(tag_data, DEFAULT_SIZE, 0);
	ByteBuffer non_existing_key_buffer = ByteBuffer_CreateAndAppendCString(non_existing_key_data, sizeof(non_existing_key_data), "this is an not existing key");
	KineticEntry getEntry = {
        .key = non_existing_key_buffer,
		.dbVersion = version_buffer,
        .tag = tag_buffer,
        .metadataOnly = true,
    };

    KineticStatus status = KineticClient_Get(Fixture.session, &getEntry, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_FOUND, status);
}
