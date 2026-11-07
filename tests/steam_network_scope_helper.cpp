// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/network_traffic.hpp"
// Model the HTTP receiver compiled separately from its downloader worker.
void receive_from_other_translation_unit(std::uint64_t n) { ww::steam_integration::traffic::received(n); }
