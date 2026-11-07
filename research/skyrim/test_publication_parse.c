/* Synthetic parser regression: no console or user data. */
#include "pw_title_publication.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char metadata[] =
"schema=3\ninstall_id=install-123\ntitle_id=FAKE12345\n"
"data_root=/data/playden/games/install-123\nprofile_id=profile-123\n"
"runtime_bundle_id=wine-wow64\nrecipe_id=recipe-123\nrecipe_version=1\n"
"settings_revision=0\nprofile_sha256=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
"descriptor_sha256=bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n"
"catalog_sha256=cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\n"
"kit_id=kit-test\ninput_count=0\ndxvk.path_hex=2f646174612f746573742f6478766b2e636f6e66\n"
"dxvk.present=0\ndxvk.sha256=\ngame_root.unowned=1\nregistry_count=0\n";
static const char state[] =
"{\"schema_version\":1,\"phase\":\"committed\",\"transaction_id\":\"00000000-0000-1000-8000-000000000001\","
"\"install_id\":\"install-123\",\"title_id\":\"FAKE12345\",\"kit_id\":\"kit-test\","
"\"runtime_bundle_id\":\"wine-wow64\",\"recipe_id\":\"recipe-123\",\"recipe_version\":\"1\","
"\"previous_revision\":0,\"settings_revision\":0,"
"\"plan_sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
"\"publication_sha256\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"}";
int main(void){PwTitlePublicationMetadata m;PwTitlePublication p;
 assert(pw_title_publication_metadata_parse((const unsigned char*)metadata,strlen(metadata),&m)==PW_OK);
 assert(m.schema_version==3 && m.game_root_unowned==1);
 assert(pw_title_publication_parse((const unsigned char*)state,strlen(state),&p)==PW_OK);
 assert(p.phase==PW_TITLE_PUBLICATION_COMMITTED);
 puts("PASS: synthetic schema 3 metadata and committed publication state parse");}
