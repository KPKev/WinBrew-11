#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/title-rule"
printf 'FAKE78005\n' > "$tmp/title-rule/allow-list"
chmod 700 "$tmp/title-rule"
chmod 600 "$tmp/title-rule/allow-list"
gcc -std=c11 -Wall -Wextra -Werror -D_DEFAULT_SOURCE \
  "-DWINE11_RULE_DIR=\"$tmp/title-rule\"" "-DWINE11_RULE_LIST=\"$tmp/title-rule/allow-list\"" \
  research/skyrim/title_rule_owner_probe.c -o "$tmp/probe"
result=$("$tmp/probe")
[[ "$result" == *"dir_present=1"* && "$result" == *"dir_private=1"* && "$result" == *"list_present=1"* && "$result" == *"list_private=1"* && "$result" == *"list_single_link=1"* && "$result" == *"stable=1"* ]]
ln "$tmp/title-rule/allow-list" "$tmp/hardlink"
result=$("$tmp/probe")
[[ "$result" == *"list_single_link=0"* ]]
rm "$tmp/hardlink"
chmod 666 "$tmp/title-rule/allow-list"
result=$("$tmp/probe")
[[ "$result" == *"list_private=0"* ]]
rm "$tmp/title-rule/allow-list"
ln -s /dev/null "$tmp/title-rule/allow-list"
result=$("$tmp/probe")
[[ "$result" == *"list_present=0"* && "$result" == *"stable=0"* ]]
echo 'PASS: read-only title-rule observation handles normal, unsafe and symlink entries'
