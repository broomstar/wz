#include <stdio.h>
#include <string.h>
#include <check.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <file.h>
#include "mem.h"

FILE *
create_tmpfile(char * buffer, size_t len) {
  FILE * raw = fopen("tmpfile", "w+b");
  ck_assert(raw != NULL);
  ck_assert(fwrite(buffer, 1, len, raw) == len);
  rewind(raw);
  return raw;
}

void
delete_tmpfile(FILE * raw) {
  ck_assert_int_eq(fclose(raw), 0);
  ck_assert_int_eq(remove("tmpfile"), 0);
}

void
create_file(wzfile * file, char * buffer, size_t len) {
  file->raw = create_tmpfile(buffer, len);
  file->pos = 0;
  file->size = len;
  file->ver.hash = 0;
  // decoded == encoded    ^ mask       ^ key
  // 'ab'    == "\x01\x23" ^ "\xaa\xab" ^ "\xca\xea"
  file->keys[0] = (wzkey) {.bytes = (uint8_t *) "\xca\xea", .len = 2};
  file->keys[1] = file->keys[0];
  file->keys[2] = file->keys[0];
  file->key = NULL;
}

void
delete_file(wzfile * file) {
  ck_assert(memerr() == 0);
  delete_tmpfile(file->raw);
}

START_TEST(test_read_data) {
  // It should be ok
  char normal[] = "ab";
  wzfile file;
  create_file(&file, normal, strlen(normal));
  char buffer[] = "cd";
  ck_assert_int_eq(wz_read_data(buffer, strlen(buffer), &file), 0);
  ck_assert_int_eq(memcmp(buffer, normal, strlen(normal)), 0);

  // It should not change position if error occured.
  file.pos = 0;
  ck_assert_int_eq(wz_read_data(buffer, strlen(buffer), &file), 1);
  ck_assert(file.pos == 0);
  delete_file(&file);

  // It should not change data if position + len > size
  create_file(&file, normal, strlen(normal));
  ck_assert_int_eq(wz_read_data(buffer, strlen(buffer), &file), 0);
  ck_assert_int_eq(wz_read_data(buffer, strlen(buffer), &file), 1);
  ck_assert_int_eq(memcmp(buffer, normal, strlen(normal)), 0);
  delete_file(&file);
} END_TEST

START_TEST(test_read_byte) {
  // It should be ok
  char normal[] = "a";
  wzfile file;
  create_file(&file, normal, strlen(normal));
  uint8_t buffer;
  ck_assert_int_eq(wz_read_byte(&buffer, &file), 0);
  ck_assert(buffer == normal[0]);

  // It should not change data if position + len > size
  uint8_t copy = buffer;
  ck_assert_int_eq(wz_read_byte(&buffer, &file), 1);
  ck_assert(buffer == copy);
  delete_file(&file);
} END_TEST

START_TEST(test_read_le16) {
  // It should be ok
  char normal[] = "\x01\x23";
  wzfile file;
  create_file(&file, normal, strlen(normal));
  uint16_t buffer;
  ck_assert_int_eq(wz_read_le16(&buffer, &file), 0);
  ck_assert(buffer == 0x2301);

  // It should not change data if position + len > size
  uint16_t copy = buffer;
  ck_assert_int_eq(wz_read_le16(&buffer, &file), 1);
  ck_assert(buffer == copy);
  delete_file(&file);
} END_TEST

START_TEST(test_read_le32) {
  // It should be ok
  char normal[] = "\x01\x23\x45\x67";
  wzfile file;
  create_file(&file, normal, strlen(normal));
  uint32_t buffer;
  ck_assert_int_eq(wz_read_le32(&buffer, &file), 0);
  ck_assert(buffer == 0x67452301);

  // It should not change data if position + len > size
  uint32_t copy = buffer;
  ck_assert_int_eq(wz_read_le32(&buffer, &file), 1);
  ck_assert(buffer == copy);
  delete_file(&file);
} END_TEST

START_TEST(test_read_le64) {
  // It should be ok
  char normal[] = "\x01\x23\x45\x67\x89\xab\xcd\xef";
  wzfile file;
  create_file(&file, normal, strlen(normal));
  uint64_t buffer;
  ck_assert_int_eq(wz_read_le64(&buffer, &file), 0);
  ck_assert(buffer == 0xefcdab8967452301);

  // It should not change data if position + len > size
  uint64_t copy = buffer;
  ck_assert_int_eq(wz_read_le64(&buffer, &file), 1);
  ck_assert(buffer == copy);
  delete_file(&file);
} END_TEST

START_TEST(test_read_int) {
  // It should read positive int8
  char normal[] = "\x01\xfe\x80\x23\x45\x67\x89";
  wzfile file;
  create_file(&file, normal, strlen(normal));
  uint32_t buffer;
  ck_assert_int_eq(wz_read_int(&buffer, &file), 0);
  ck_assert(buffer == 1);

  // It should read negative int8
  ck_assert_int_eq(wz_read_int(&buffer, &file), 0);
  ck_assert(buffer == 0xfffffffe);

  // It should read postive int32
  ck_assert_int_eq(wz_read_int(&buffer, &file), 0);
  ck_assert(buffer == 0x89674523);

  // It should not change data if position + len > size
  uint32_t copy = buffer;
  ck_assert_int_eq(wz_read_int(&buffer, &file), 1);
  ck_assert(buffer == copy);
  delete_file(&file);
} END_TEST

START_TEST(test_read_long) {
  // It should read positive int8
  char normal[] = "\x01\xfe\x80\x23\x45\x67\x89\xab\xcd\xef\01";
  wzfile file;
  create_file(&file, normal, strlen(normal));
  uint64_t buffer;
  ck_assert_int_eq(wz_read_long(&buffer, &file), 0);
  ck_assert(buffer == 1);

  // It should read negative int8
  ck_assert_int_eq(wz_read_long(&buffer, &file), 0);
  ck_assert(buffer == 0xfffffffffffffffe);

  // It should read postive int64
  ck_assert_int_eq(wz_read_long(&buffer, &file), 0);
  ck_assert(buffer == 0x01efcdab89674523);

  // It should not change data if position + len > size
  uint64_t copy = buffer;
  ck_assert_int_eq(wz_read_long(&buffer, &file), 1);
  ck_assert(buffer == copy);
  delete_file(&file);
} END_TEST

START_TEST(test_read_bytes) {
  // It should be ok
  char normal[] = "ab";
  wzfile file;
  create_file(&file, normal, strlen(normal));
  char buffer[] = "cd";
  ck_assert_int_eq(wz_read_bytes((uint8_t *) buffer, strlen(buffer), &file), 0);
  ck_assert_int_eq(memcmp(buffer, normal, strlen(normal)), 0);

  // It should not change data if position + len > size
  ck_assert_int_eq(wz_read_bytes((uint8_t *) buffer, strlen(buffer), &file), 1);
  ck_assert_int_eq(memcmp(buffer, normal, strlen(normal)), 0);
  delete_file(&file);
} END_TEST

START_TEST(test_init_str) {
  // It should be ok
  wzstr buffer;
  wz_init_str(&buffer);
  ck_assert(buffer.bytes == NULL);
} END_TEST

START_TEST(test_read_str) {
  // It should be ok
  char normal[] = "ab";
  wzfile file;
  create_file(&file, normal, strlen(normal));
  wzstr buffer;
  wz_init_str(&buffer);
  ck_assert_int_eq(wz_read_str(&buffer, strlen(normal), &file), 0);
  ck_assert_int_eq(memcmp(buffer.bytes, normal, strlen(normal)), 0);
  ck_assert(buffer.len == strlen(normal));
  ck_assert(memused() == strlen(normal));

  // It should not change data if position + len > size
  wzstr copy = buffer;
  ck_assert_int_eq(wz_read_str(&buffer, strlen(normal), &file), 1);
  ck_assert(buffer.bytes == copy.bytes);
  ck_assert(buffer.len == copy.len);

  // It should not allocate memory if position + len > size
  ck_assert(memused() == strlen(normal));
  wz_free_str(&buffer);
  delete_file(&file);
} END_TEST

START_TEST(test_free_str) {
  // It should be ok
  char normal[] = "ab";
  wzfile file;
  create_file(&file, normal, strlen(normal));
  wzstr buffer;
  wz_init_str(&buffer);
  ck_assert_int_eq(wz_read_str(&buffer, strlen(normal), &file), 0);
  ck_assert(memused() == strlen(normal));
  wz_free_str(&buffer);
  ck_assert(memused() == 0);
  delete_file(&file);
} END_TEST

START_TEST(test_decode_chars) {
  // It should decode ascii
  char ascii[] = "\x01\x23";
  wzfile file = {
    .key = &file.keys[0],
    .keys = {{.bytes = (uint8_t *) "\x89\xab\xcd\xef", .len = 4}}
  };
  wzchr buffer;
  buffer.bytes = (uint8_t *) ascii;
  buffer.len = strlen(ascii);
  buffer.enc = WZ_ENC_ASCII;
  ck_assert_int_eq(wz_decode_chars(&buffer, file.key), 0);
  ck_assert_int_eq(memcmp(buffer.bytes, "\x22\x23", 2), 0);
  ck_assert(buffer.len == 2 && memused() == 0);

  // It should decode utf16le
  char utf16le[] = "\x45\x67";
  buffer.bytes = (uint8_t *) utf16le;
  buffer.len = strlen(utf16le);
  buffer.enc = WZ_ENC_UTF16LE;
  ck_assert_int_eq(wz_decode_chars(&buffer, file.key), 0);
  ck_assert_int_eq(memcmp(buffer.bytes, "\x66\x66", 2), 0);
  ck_assert(buffer.len == 2 && memused() == 0);

  // It should not decode if key == NULL
  wzchr copy = buffer;
  ck_assert_int_eq(wz_decode_chars(&buffer, NULL), 0);
  ck_assert(buffer.bytes == copy.bytes);
  ck_assert(buffer.len == copy.len);

  // It should not decode if string key is too short
  file.key->bytes = (uint8_t *) "\xcd";
  file.key->len = 1;
  ck_assert_int_eq(wz_decode_chars(&buffer, file.key), 1);
  ck_assert(buffer.bytes == copy.bytes);
  ck_assert(buffer.len == copy.len);
} END_TEST

START_TEST(test_read_chars) {
  // It should be ok
  char normal[] =
    "\xfe""\x01\x23"
    "\x80""\x02\x00\x00\x00""\x45\x67"
    "\x01""\x89\xab"
    "\x7f""\x03\x00\x00\x00""\xcd\xef\x01\x23\x45\x67";
  wzfile file;
  create_file(&file, normal, sizeof(normal) - 1);
  wzchr buffer;
  ck_assert_int_eq(wz_read_chars(&buffer, file.key, &file), 0);
  ck_assert_int_eq(memcmp(buffer.bytes, "\x01\x23", 2), 0);
  ck_assert(buffer.len == 2 && memused() == 2);
  wz_free_chars(&buffer);
  ck_assert_int_eq(wz_read_chars(&buffer, file.key, &file), 0);
  ck_assert_int_eq(memcmp(buffer.bytes, "\x45\x67", 2), 0);
  ck_assert(buffer.len == 2 && memused() == 2);
  wz_free_chars(&buffer);
  ck_assert_int_eq(wz_read_chars(&buffer, file.key, &file), 0);
  ck_assert_int_eq(memcmp(buffer.bytes, "\x89\xab", 2), 0);
  ck_assert(buffer.len == 2 && memused() == 2);
  wz_free_chars(&buffer);
  ck_assert_int_eq(wz_read_chars(&buffer, file.key, &file), 0);
  char * expected = "\xcd\xef\x01\x23\x45\x67";
  ck_assert_int_eq(memcmp(buffer.bytes, expected, 6), 0);
  ck_assert(buffer.len == 6 && memused() == 6);
  wz_free_chars(&buffer);
  delete_file(&file);

  // It should decode if string key is set
  create_file(&file, normal, sizeof(normal) - 1);
  file.key = &file.keys[0];
  file.key->bytes = (uint8_t *) "\x01\x23\x45\x67\x89\xab";
  file.key->len = 6;
  ck_assert_int_eq(wz_read_chars(&buffer, file.key, &file), 0);
  ck_assert_int_eq(memcmp(buffer.bytes, "\xaa\xab", 2), 0);
  ck_assert(buffer.len == 2 && memused() == 2);
  wz_free_chars(&buffer);
  ck_assert_int_eq(wz_read_chars(&buffer, file.key, &file), 0);
  ck_assert_int_eq(memcmp(buffer.bytes, "\xee\xef", 2), 0);
  ck_assert(buffer.len == 2 && memused() == 2);
  wz_free_chars(&buffer);
  ck_assert_int_eq(wz_read_chars(&buffer, file.key, &file), 0);
  ck_assert_int_eq(memcmp(buffer.bytes, "\x22\x22", 2), 0);
  ck_assert(buffer.len == 2 && memused() == 2);
  wz_free_chars(&buffer);
  ck_assert_int_eq(wz_read_chars(&buffer, file.key, &file), 0);
  ck_assert_int_eq(memcmp(buffer.bytes, "\x66\x66\xef\xee\x60\x66", 6), 0);
  ck_assert(buffer.len == 6 && memused() == 6);
  wz_free_chars(&buffer);
  delete_file(&file);
} END_TEST

START_TEST(test_free_chars) {
  // It should be ok
  char normal[] = "\xfe\x01\x23";
  wzfile file;
  create_file(&file, normal, sizeof(normal) - 1);
  wzchr buffer;
  ck_assert_int_eq(wz_read_chars(&buffer, file.key, &file), 0);
  ck_assert(memused() == 2);
  wz_free_chars(&buffer);
  ck_assert(memused() == 0);
  delete_file(&file);
} END_TEST

START_TEST(test_rotl32) {
  // It should be ok
  ck_assert(wz_rotl32(0xf2345678, 3) == 0x91a2b3c7);
} END_TEST

START_TEST(test_decode_addr) {
  // It should be ok
  wzfile file = {.head = {.start = 0x3c}, .ver = {.hash = 0x713}};
  wzaddr addr = {.pos = 0x51, .val = 0x49e34db3};
  wz_decode_addr(&addr, &file);
  ck_assert(addr.val == 0x2ed);
} END_TEST

START_TEST(test_read_addr) {
  // It should be ok
  char normal[] =
    "\x01\x23\x45\x67""\x89\xab\xcd\xef"
    "\x01\x23\x45\x67";
  wzfile file;
  create_file(&file, normal, strlen(normal));
  wzaddr addr;
  ck_assert_int_eq(wz_read_addr(&addr, &file), 0);
  ck_assert(addr.val == 0x67452301 && addr.pos == 0);
  ck_assert_int_eq(wz_read_addr(&addr, &file), 0);
  ck_assert(addr.val == 0xefcdab89 && addr.pos == 4);

  // It should decode address if hash is present
  file.head.start = 0x12;
  file.ver.hash = 0x89abcdef;
  ck_assert_int_eq(wz_read_addr(&addr, &file), 0);
  ck_assert(addr.val == 0x8ebe951a && addr.pos == 8);
  delete_file(&file);
} END_TEST

START_TEST(test_seek) {
  // It should seek absolute address
  char normal[] =
    "\x01\x23\x45\x67\x89";
  wzfile file;
  create_file(&file, normal, strlen(normal));
  ck_assert_int_eq(wz_seek(2, SEEK_SET, &file), 0);
  ck_assert(file.pos == 2);
  ck_assert(ftell(file.raw) == 2);

  // It should seek relative address
  ck_assert_int_eq(wz_seek(3, SEEK_CUR, &file), 0);
  ck_assert(file.pos == 5);
  ck_assert(ftell(file.raw) == 5);
  delete_file(&file);
} END_TEST

START_TEST(test_read_node) {
  // It should read type 1
  char normal[] =
    "\x01""\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\x02""\x29\x00\x00\x00""\x01""\x02""\x01\x23\x45\x67"
    "\x03""\xfe\x01\x23""\x01""\x02""\x01\x23\x45\x67"
    "\x04""\xfe\x01\x23""\x01""\x02""\x01\x23\x45\x67"
    "\x05"
    "\x03""\xfe\x01\x23"; // type 2 string
  wzfile file;
  create_file(&file, normal, sizeof(normal) - 1);
  wznode node;
  ck_assert_int_eq(wz_read_node(&node, &file), 0);
  ck_assert(memused() == 0);

  // It should read type 2
  file.head.start = 0x2;
  ck_assert(memused() == 0);
  ck_assert_int_eq(wz_read_node(&node, &file), 0);
  ck_assert(node.type == 3);
  ck_assert_int_eq(memcmp(node.name.bytes, "ab", 2), 0);
  ck_assert(node.name.len == 2);
  ck_assert(node.size == 1 && node.check == 2 && node.addr.val == 0x67452301);
  ck_assert(memused() == 2);
  wz_free_node(&node);

  // It should read type 3
  ck_assert(memused() == 0);
  ck_assert_int_eq(wz_read_node(&node, &file), 0);
  ck_assert(node.type == 3);
  ck_assert_int_eq(memcmp(node.name.bytes, "ab", 2), 0);
  ck_assert(node.name.len == 2);
  ck_assert(node.size == 1 && node.check == 2 && node.addr.val == 0x67452301);
  ck_assert(memused() == 2);
  wz_free_node(&node);

  // It should read type 4
  ck_assert(memused() == 0);
  ck_assert_int_eq(wz_read_node(&node, &file), 0);
  ck_assert(node.type == 4);
  ck_assert_int_eq(memcmp(node.name.bytes, "ab", 2), 0);
  ck_assert(node.name.len == 2);
  ck_assert(node.size == 1 && node.check == 2 && node.addr.val == 0x67452301);
  ck_assert(memused() == sizeof(* node.data.var) + 2);
  wz_free_node(&node);

  // It should not read type 5
  ck_assert_int_eq(wz_read_node(&node, &file), 1);
  ck_assert(node.type == 5);
  ck_assert(memused() == 0);
  delete_file(&file);
} END_TEST

START_TEST(test_free_node) {
  // It should be ok
  char normal[] = "\x03""\xfe\x01\x23""\x01""\x02""\x01\x23\x45\x67";
  wzfile file;
  create_file(&file, normal, strlen(normal));
  wznode node;
  ck_assert_int_eq(wz_read_node(&node, &file), 0);
  ck_assert(memused() == 2);
  wz_free_node(&node);
  ck_assert(memused() == 0);
  delete_file(&file);
} END_TEST

START_TEST(test_decode_node) {
  // It should be ok
  wzfile file = {
    .head = {.start = 0x3c},
    .ver = {.hash = 0x713}
  };
  wznode node = {
    .type = 3,
    .addr = {.pos = 0x51, .val = 0x49e34db3}
  };
  ck_assert_int_eq(wz_decode_node(&node, &file), 0);
  ck_assert(node.addr.val == 0x2ed);
  ck_assert(memused() == 0);
} END_TEST

START_TEST(test_read_grp) {
  // It should be ok
  char normal[] = "\x03"
    "\x01""\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\x03""\xfe\x01\x23""\x01""\x02""\x01\x23\x45\x67"
    "\x04""\xfe\x01\x23""\x01""\x02""\x01\x23\x45\x67";
  wzfile file;
  create_file(&file, normal, strlen(normal));
  wzgrp * grp = NULL;
  ck_assert_int_eq(wz_read_grp(&grp, NULL, &file), 0);
  ck_assert(grp->len == 3);
  ck_assert(grp->nodes[0].type == 1);
  ck_assert(grp->nodes[1].type == 3);
  ck_assert(grp->nodes[2].type == 4);
  ck_assert(memused() ==
            sizeof(* grp) +
            sizeof(* grp->nodes) * 3 +
            sizeof(* grp->nodes[0].data.var) +
            2 * 2);
  wz_free_grp(&grp);
  delete_file(&file);

  // It should not read invalid data
  char error[] = "\x03"
    "\x01""\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\x03""\xfe\x01\x23""\x01""\x02""\x01\x23\x45\x67"
    "\x02";
  create_file(&file, error, strlen(error));
  ck_assert_int_eq(wz_read_grp(&grp, NULL, &file), 1);
  ck_assert(memused() == 0);
  delete_file(&file);
} END_TEST

START_TEST(test_free_grp) {
  // It should be ok
  char normal[] = "\x01"
    "\x03""\xfe\x01\x23""\x01""\x02""\x01\x23\x45\x67";
  wzfile file;
  create_file(&file, normal, strlen(normal));
  wzgrp * grp = NULL;
  ck_assert_int_eq(wz_read_grp(&grp, NULL, &file), 0);
  ck_assert(memused() == sizeof(* grp) + sizeof(* grp->nodes) + 2);
  wz_free_grp(&grp);
  ck_assert(memused() == 0);
  delete_file(&file);
} END_TEST

START_TEST(test_decode_grp) {
  // It should be ok
  wzfile file = {
    .head = {.start = 0x3c},
    .ver = {.hash = 0x713}
  };
  wznode node = {
    .type = 3,
    .addr = {.pos = 0x51, .val = 0x49e34db3}
  };
  wzgrp grp = {.len = 1, .nodes = &node};
  ck_assert_int_eq(wz_decode_grp(&grp, &file), 0);
  ck_assert(node.addr.val == 0x2ed);
  ck_assert(memused() == 0);
} END_TEST

START_TEST(test_read_head) {
  // It should be ok
  char normal[] =
    "\x01\x23\x45\x67"
    "\x12\x00\x00\x00\x00\x00\x00\x00"
    "\x12\x00\x00\x00"
    "ab";
  wzfile file;
  create_file(&file, normal, sizeof(normal) - 1);
  wzhead head;
  ck_assert_int_eq(wz_read_head(&head, &file), 0);
  ck_assert_int_eq(memcmp(head.ident, "\x01\x23\x45\x67", 4), 0);
  ck_assert(head.size == 18 && head.start == 18);
  ck_assert_int_eq(memcmp(head.copy.bytes, "ab", 2), 0);
  ck_assert(head.copy.len == 2);
  ck_assert(memused() == 2);
  wz_free_head(&head);
  delete_file(&file);

  // It should not read invalid data
  char error[] =
    "\x01\x23\x45\x67"
    "\x12\x00\x00\x00\x00\x00\x00\x00"
    "\x12\x00\x00\x00"
    "a";
  create_file(&file, error, sizeof(error) - 1);
  ck_assert_int_eq(wz_read_head(&head, &file), 1);
  ck_assert(memused() == 0);
  delete_file(&file);
} END_TEST

START_TEST(test_free_head) {
  // It should be ok
  char normal[] =
    "\x01\x23\x45\x67"
    "\x12\x00\x00\x00\x00\x00\x00\x00"
    "\x12\x00\x00\x00"
    "ab";
  wzfile file;
  create_file(&file, normal, sizeof(normal) - 1);
  wzhead head;
  ck_assert_int_eq(wz_read_head(&head, &file), 0);
  ck_assert(memused() == 2);
  wz_free_head(&head);
  ck_assert(memused() == 0);
  delete_file(&file);
} END_TEST

START_TEST(test_encode_ver) {
  // It should be ok
  wzver ver = {.dec = 0x0123};
  ck_assert_int_eq(wz_encode_ver(&ver), 0);
  ck_assert(ver.enc == 0x005e && ver.hash == 0xd372);
} END_TEST

START_TEST(test_valid_ver) {
  // It should be ok
  wzfile file = {
    .size = 0x8a95328,
    .head = {.start = 0x3c},
    .root = {
      .data = {
        .grp = (wzgrp[]) {{
          .len = 1,
          .nodes = (wznode[]) {{
            .type = 3,
            .addr = {.pos = 0x51, .val = 0x49e34db3}
          }}
        }}
      }
    },
    .ver = {.hash = 0x99}
  };
  wzver ver = {.hash = 0x713};
  ck_assert_int_eq(wz_valid_ver(&ver, &file.root, &file), 0);
  ck_assert(file.ver.hash == 0x99);

  // It should not change version hash if failed
  ver.hash = 0x712;
  ck_assert_int_eq(wz_valid_ver(&ver, &file.root, &file), 1);
  ck_assert(file.ver.hash == 0x99);
} END_TEST

START_TEST(test_deduce_ver) {
  // It should be ok
  wzfile file = {
    .size = 0x712e04f2,
    .head = {.start = 0x3c},
    .root = {
      .data = {
        .grp = (wzgrp[]) {{
          .len = 1,
          .nodes = (wznode[]) {{
            .type = 3,
            .addr = {.pos = 0x56, .val = 0x5eb2cd05}
          }}
        }}
      }
    },
    .ver = {.hash = 0x99}
  };
  wzver ver = {.enc = 0x93};
  ck_assert_int_eq(wz_deduce_ver(&ver, &file), 0);
  ck_assert(ver.enc == 0x93);
  ck_assert(ver.dec == 0x153);
  ck_assert(ver.hash == 0xd6ba);
} END_TEST

START_TEST(test_alloc_crypto) {
  // It shoule be ok
  ck_assert_int_eq(wz_alloc_crypto(), 0);
  EVP_CIPHER_CTX * ctx = EVP_CIPHER_CTX_new();
  ck_assert(ctx != NULL);
  EVP_CIPHER_CTX_free(ctx);
  wz_dealloc_crypto();
} END_TEST

START_TEST(test_dealloc_crypto) {
  // It shoule be ok
  ck_assert_int_eq(wz_alloc_crypto(), 0);
  EVP_CIPHER_CTX * ctx = EVP_CIPHER_CTX_new();
  ck_assert(ctx != NULL);
  EVP_CIPHER_CTX_free(ctx);
  wz_dealloc_crypto();
  ck_assert_int_eq(wz_alloc_crypto(), 0);
  ctx = EVP_CIPHER_CTX_new();
  ck_assert(ctx != NULL);
  EVP_CIPHER_CTX_free(ctx);
  wz_dealloc_crypto();
} END_TEST

START_TEST(test_decode_aes) {
  // It shoule be ok
  ck_assert_int_eq(wz_alloc_crypto(), 0);
  uint8_t key[32] =
    "\x13\x00\x00\x00\x08\x00\x00\x00""\x06\x00\x00\x00\xb4\x00\x00\x00"
    "\x1b\x00\x00\x00\x0f\x00\x00\x00""\x33\x00\x00\x00\x52\x00\x00\x00";
  uint8_t iv[16] =
    "\x4d\x23\xc7\x2b\x4d\x23\xc7\x2b""\x4d\x23\xc7\x2b\x4d\x23\xc7\x2b";
  uint8_t cipher[16] =
    "\x96\xae\x3f\xa4\x48\xfa\xdd\x90""\x46\x76\x05\x61\x97\xce\x78\x68";
  uint8_t plain[16];
  memset(plain, 0x11, sizeof(plain));
  ck_assert_int_eq(wz_decode_aes(plain, cipher, 16, key, iv), 0);
  uint8_t expected[16] = {0};
  ck_assert_int_eq(memcmp(plain, expected, 16), 0);
  wz_dealloc_crypto();
} END_TEST

START_TEST(test_encode_aes) {
  // It shoule be ok
  ck_assert_int_eq(wz_alloc_crypto(), 0);
  uint8_t plain[32] =
    "\x00\x00\x00\x00\x00\x00\x00\x00""\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00""\x00\x00\x00\x00\x00\x00\x00\x00";
  uint8_t cipher[32] = {0};
  wzaes aes = {
    .key =
      "\x13\x00\x00\x00\x08\x00\x00\x00""\x06\x00\x00\x00\xb4\x00\x00\x00"
      "\x1b\x00\x00\x00\x0f\x00\x00\x00""\x33\x00\x00\x00\x52\x00\x00\x00",
    .iv =
      "\x4d\x23\xc7\x2b\x4d\x23\xc7\x2b""\x4d\x23\xc7\x2b\x4d\x23\xc7\x2b",
    .plain = plain, .cipher = cipher, .len = 32
  };
  ck_assert_int_eq(wz_encode_aes(&aes), 0);
  uint8_t expected[32] =
    "\x96\xae\x3f\xa4\x48\xfa\xdd\x90""\x46\x76\x05\x61\x97\xce\x78\x68"
    "\x2b\xa0\x44\x8f\xc1\x56\x7e\x32""\xfc\xe1\xf5\xb3\x14\x14\xc5\x22";
  ck_assert_int_eq(memcmp(cipher, expected, 32), 0);
  wz_dealloc_crypto();
} END_TEST

START_TEST(test_init_aes) {
  // It should be ok
  wzaes aes;
  memset(&aes, 0, sizeof(aes));
  ck_assert_int_eq(wz_init_aes(&aes), 0);
  ck_assert(aes.key[0] != 0);
  ck_assert(aes.len != 0);
  ck_assert(memused() == aes.len * 2);
  ck_assert(aes.plain != NULL);
  ck_assert(aes.cipher != NULL);
  wz_free_aes(&aes);
} END_TEST

START_TEST(test_free_aes) {
  // It should be ok
  wzaes aes;
  memset(&aes, 0, sizeof(aes));
  ck_assert_int_eq(wz_init_aes(&aes), 0);
  ck_assert(memused() == aes.len * 2);
  wz_free_aes(&aes);
  ck_assert(memused() == 0);
} END_TEST

START_TEST(test_init_key) {
  // It should be ok
  wzaes aes;
  ck_assert_int_eq(wz_init_aes(&aes), 0);
  wzkey key;
  ck_assert_int_eq(wz_init_key(&key, &aes), 0);
  ck_assert(memused() == aes.len * 3);
  ck_assert(key.bytes != NULL);
  wz_free_key(&key);
  wz_free_aes(&aes);
} END_TEST

//START_TEST(test_valid_nodekey) {
//  // It should be ok
//  wzfile file = {
//    .root = {
//      .data = {
//        .dir = (wzdir[]) {{
//          .len = 2,
//          .nodes = (wznode[]) {{
//            .type = 0x03,
//            .name = {.len = 3, .bytes = "a0C", .enc = WZ_ENC_ASCII}
//          }, {
//            .type = 0x04,
//            .name = {.len = 3, .bytes = "<?@", .enc = WZ_ENC_ASCII}
//          }}
//        }}
//      }
//    }
//  };
//  wzstrk strk = {.ascii = NULL};
//  ck_assert_int_eq(wz_valid_nodekey(&strk, &file), 0);
//
//  // It should fail if any character cannot be decoded
//  file.root.data.dir->nodes[1].name.bytes = "<?\x80";
//  ck_assert_int_eq(wz_valid_nodekey(&strk, &file), 1);
//} END_TEST

START_TEST(test_set_key) {
  // It should be ok
  wzaes aes = {
    .cipher = (uint8_t *)
      "\x96\xae\x3f\xa4\x48\xfa\xdd\x90""\x46\x76\x05\x61\x97\xce\x78\x68"
      "\x2b\xa0\x44\x8f\xc1\x56\x7e\x32""\xfc\xe1\xf5\xb3\x14\x14\xc5\x22",
    .len = 32
  };
  wzkey key;
  ck_assert_int_eq(wz_init_key(&key, &aes), 0);
  wz_set_key(&key, &aes);
  ck_assert_int_eq(memcmp(key.bytes, aes.cipher, 32), 0);
  ck_assert(key.len == aes.len);
  wz_free_key(&key);
} END_TEST

START_TEST(test_free_key) {
  // It should be ok
  wzaes aes;
  ck_assert_int_eq(wz_init_aes(&aes), 0);
  wzkey key;
  ck_assert_int_eq(wz_init_key(&key, &aes), 0);
  ck_assert(memused() == aes.len * 3);
  ck_assert(memused() > 0);
  wz_free_key(&key);
  ck_assert(memused() == aes.len * 2);
  ck_assert(memused() > 0);
  wz_free_aes(&aes);
} END_TEST

//START_TEST(test_decode_strk) {
//  // It should decode GMS
//  uint8_t gms[] = "\x6f\x6d\xfa\x6c\x8a\x31";
//  wzfile file = {
//    .root = {
//      .data = {
//        .dir = (wzdir[]) {{
//          .len = 1,
//          .nodes = (wznode[]) {{
//            .type = 0x03,
//            .name = {.len = strlen(gms), .bytes = gms, .enc = WZ_ENC_ASCII}
//          }}
//        }}
//      }
//    }
//  };
//  wzstrk strk;
//  ck_assert_int_eq(wz_decode_strk(&strk, wz_valid_nodekey, &file), 0);
//  ck_assert(memused() == strk.len * 2);
//  ck_assert(memused() > 0);
//  wz_free_strk(&strk);
//
//  // It should decode TMS
//  uint8_t tms[] = "\x31\xfe\xd5\x99\xfb\x52";
//  file.root.data.dir->nodes[0].name.bytes = tms;
//  ck_assert_int_eq(wz_decode_strk(&strk, wz_valid_nodekey, &file), 0);
//  ck_assert(memused() == strk.len * 2);
//  ck_assert(memused() > 0);
//  wz_free_strk(&strk);
//
//  // It should decode JMS
//  uint8_t jms[] = "\x9a\x9b\x9c\x9d\x9c\x9f";
//  file.root.data.dir->nodes[0].name.bytes = jms;
//  ck_assert_int_eq(wz_decode_strk(&strk, wz_valid_nodekey, &file), 0);
//  ck_assert(memused() == strk.len * 2);
//  ck_assert(memused() > 0);
//  wz_free_strk(&strk);
//} END_TEST

START_TEST(test_read_file) {
  // It should be ok
  char normal[] =
    "\x01\x23\x45\x67"                 // ident
    "\x1F\x00\x00\x00\x00\x00\x00\x00" // size
    "\x12\x00\x00\x00"                 // start
    "ab"                               // copy
    "\x7a\x00"                         // ver
    "\x01"                             // nodes len
    "\x03""\xfe\x5d\x67""\x01""\x02""\x27\x4b\xda\x8e";
  // "\x5d\x67" == "\x96\xae" ^ "\xaa\xab" ^ "ab"
  // version and address is generated by 'encode' program
  wzfile file;
  FILE * raw = create_tmpfile(normal, sizeof(normal) - 1);
  ck_assert_int_eq(wz_read_file(&file, raw), 0);
  ck_assert_int_eq(memcmp(file.head.copy.bytes, "ab", 2), 0);
  ck_assert(file.head.copy.len == 2);
  wzaes aes;
  ck_assert_int_eq(wz_init_aes(&aes), 0);
  wz_free_aes(&aes);
  ck_assert(memused() == aes.len * 3 + 2);
  wz_free_file(&file);
  delete_tmpfile(raw);

  // It should not read invalid data
  char error[] =
    "\x01\x23\x45\x67"
    "\x1F\x00\x00\x00\x00\x00\x00\x00"
    "\x12\x00\x00\x00"
    "ab"
    "\x01\x23"
    "\x01"
    "\x03""\xfe\x01\x23""\x01""\x02""\x01\x23\x45";
  raw = create_tmpfile(error, sizeof(error) - 1);
  ck_assert(raw != NULL);
  ck_assert_int_eq(wz_read_file(&file, raw), 1);
  ck_assert(memused() == 0);
  delete_tmpfile(raw);
} END_TEST

START_TEST(test_free_file) {
  // It should be ok
  char normal[] =
    "\x01\x23\x45\x67"                 // ident
    "\x1F\x00\x00\x00\x00\x00\x00\x00" // size
    "\x12\x00\x00\x00"                 // start
    "ab"                               // copy
    "\x7a\x00"                         // ver
    "\x01"                             // nodes len
    "\x03""\xfe\x5d\x67""\x01""\x02""\x27\x4b\xda\x8e";
  // "\x5d\x67" == "\x96\xae" ^ "\xaa\xab" ^ "ab"
  // version and address is generated by 'encode' program
  wzfile file;
  FILE * raw = create_tmpfile(normal, sizeof(normal) - 1);
  ck_assert_int_eq(wz_read_file(&file, raw), 0);
  wzaes aes;
  ck_assert_int_eq(wz_init_aes(&aes), 0);
  wz_free_aes(&aes);
  ck_assert(memused() == aes.len * 3 + 2);
  wz_free_file(&file);
  ck_assert(memused() == 0);
  delete_tmpfile(raw);
} END_TEST

START_TEST(test_open_file) {
  // It should be ok
  char * filename = "test_open_file.wz";
  FILE * raw = fopen(filename, "wb");
  ck_assert(raw != NULL);
  wzfile file;
  char normal[] =
    "\x01\x23\x45\x67"                 // ident
    "\x1F\x00\x00\x00\x00\x00\x00\x00" // size
    "\x12\x00\x00\x00"                 // start
    "ab"                               // copy
    "\x7a\x00"                         // ver
    "\x01"                             // nodes len
    "\x03""\xfe\x5d\x67""\x01""\x02""\x27\x4b\xda\x8e";
  // "\x5d\x67" == "\x96\xae" ^ "\xaa\xab" ^ "ab"
  // version and address is generated by 'encode' program
  ck_assert(fwrite(normal, 1, sizeof(normal) - 1, raw) == sizeof(normal) - 1);
  ck_assert_int_eq(fclose(raw), 0);
  ck_assert_int_eq(wz_open_file(&file, filename), 0);
  wzaes aes;
  ck_assert_int_eq(wz_init_aes(&aes), 0);
  wz_free_aes(&aes);
  ck_assert(memused() == aes.len * 3 + 2);
  ck_assert_int_eq(wz_close_file(&file), 0);
  ck_assert_int_eq(remove(filename), 0);

  // It should not read the file which does not exist
  ck_assert_int_eq(wz_open_file(&file, "not-exist-file"), 1);
} END_TEST

START_TEST(test_close_file) {
  // It should be ok
  char * filename = "test_close_file.wz";
  FILE * raw = fopen(filename, "wb");
  ck_assert(raw != NULL);
  wzfile file;
  char normal[] =
    "\x01\x23\x45\x67"                 // ident
    "\x1F\x00\x00\x00\x00\x00\x00\x00" // size
    "\x12\x00\x00\x00"                 // start
    "ab"                               // copy
    "\x7a\x00"                         // ver
    "\x01"                             // nodes len
    "\x03""\xfe\x5d\x67""\x01""\x02""\x27\x4b\xda\x8e";
  // "\x5d\x67" == "\x96\xae" ^ "\xaa\xab" ^ "ab"
  // version and address is generated by 'encode' program
  ck_assert(fwrite(normal, 1, sizeof(normal) - 1, raw) == sizeof(normal) - 1);
  ck_assert_int_eq(fclose(raw), 0);
  ck_assert_int_eq(wz_open_file(&file, filename), 0);
  wzaes aes;
  ck_assert_int_eq(wz_init_aes(&aes), 0);
  wz_free_aes(&aes);
  ck_assert(memused() == aes.len * 3 + 2);
  ck_assert_int_eq(wz_close_file(&file), 0);
  ck_assert_int_eq(remove(filename), 0);
  ck_assert(memused() == 0);
} END_TEST

Suite *
make_file_suite(void) {
  Suite * suite = suite_create("wzfile");
  TCase * tcase = tcase_create("parse_file");
  tcase_add_test(tcase, test_read_data);
  tcase_add_test(tcase, test_read_byte);
  tcase_add_test(tcase, test_read_le16);
  tcase_add_test(tcase, test_read_le32);
  tcase_add_test(tcase, test_read_le64);
  tcase_add_test(tcase, test_read_int);
  tcase_add_test(tcase, test_read_long);
  tcase_add_test(tcase, test_read_bytes);
  tcase_add_test(tcase, test_init_str);
  tcase_add_test(tcase, test_read_str);
  tcase_add_test(tcase, test_free_str);
  tcase_add_test(tcase, test_decode_chars);
  tcase_add_test(tcase, test_read_chars);
  tcase_add_test(tcase, test_free_chars);
  tcase_add_test(tcase, test_rotl32);
  tcase_add_test(tcase, test_decode_addr);
  tcase_add_test(tcase, test_read_addr);
  tcase_add_test(tcase, test_seek);
  tcase_add_test(tcase, test_read_node);
  tcase_add_test(tcase, test_free_node);
  tcase_add_test(tcase, test_decode_node);
  tcase_add_test(tcase, test_read_grp);
  tcase_add_test(tcase, test_free_grp);
  tcase_add_test(tcase, test_decode_grp);
  tcase_add_test(tcase, test_read_head);
  tcase_add_test(tcase, test_free_head);
  tcase_add_test(tcase, test_encode_ver);
  tcase_add_test(tcase, test_valid_ver);
  tcase_add_test(tcase, test_deduce_ver);
  tcase_add_test(tcase, test_alloc_crypto);
  tcase_add_test(tcase, test_dealloc_crypto);
  tcase_add_test(tcase, test_decode_aes);
  tcase_add_test(tcase, test_encode_aes);
  tcase_add_test(tcase, test_init_aes);
  tcase_add_test(tcase, test_free_aes);
  tcase_add_test(tcase, test_init_key);
  //tcase_add_test(tcase, test_valid_nodekey);
  tcase_add_test(tcase, test_set_key);
  tcase_add_test(tcase, test_free_key);
  //tcase_add_test(tcase, test_decode_strk);
  tcase_add_test(tcase, test_read_file);
  tcase_add_test(tcase, test_free_file);
  tcase_add_test(tcase, test_open_file);
  tcase_add_test(tcase, test_close_file);
  suite_add_tcase(suite, tcase);
  return suite;
}