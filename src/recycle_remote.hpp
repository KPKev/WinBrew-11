// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace ww::fileops { struct Purge;struct CrossVolumeRecycle;struct Clipboard;
bool remote_purge_prepare(Purge &,const char *,const char *);
bool remote_purge_begin(Purge &);void remote_purge_step(Purge &);void remote_purge_cancel(Purge &);void remote_purge_close(Purge &);
bool remote_transfer_begin(CrossVolumeRecycle &,const Clipboard &,const char *,bool,bool);
void remote_transfer_step(CrossVolumeRecycle &);void remote_transfer_cancel(CrossVolumeRecycle &);void remote_transfer_close(CrossVolumeRecycle &);
}
