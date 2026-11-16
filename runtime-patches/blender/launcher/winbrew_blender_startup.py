# SPDX-License-Identifier: GPL-3.0-or-later
"""Blender-only compatibility policy; embedded in the managed launcher.
Deletion means a recoverable rename into WinBrew's existing recycle format.
No unlink fallback, recursive removal, external path access or project rewriting.
"""
import os
import stat
import types
import uuid

ROOTS = tuple((r'Z:\data\WinBrew\Users\Default' + '\\' + name,
               '/data/WinBrew/Users/Default/' + name) for name in ('Documents', 'Desktop'))


def canonical(path):
    path = os.fsdecode(path)
    for name in ('Documents', 'Desktop'):
        alias = 'C:\\users\\prospero\\' + name
        if path.lower() == alias.lower() or path.lower().startswith(alias.lower() + '\\'):
            path = ROOTS[0 if name == 'Documents' else 1][0] + path[len(alias):]
            break
    return os.path.abspath(path)


def inspect_path(path):
    info = os.lstat(path)
    if stat.S_ISLNK(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400:
        raise ValueError('Linked files and folders cannot be recycled here')
    if not (stat.S_ISREG(info.st_mode) or stat.S_ISDIR(info.st_mode)):
        raise ValueError('Unsupported file type')
    return (info.st_dev, info.st_ino, info.st_mode, info.st_size, info.st_mtime_ns)


def approved(path):
    path = canonical(path)
    for root, unix_root in ROOTS:
        root = os.path.abspath(root)
        if os.path.normcase(path).startswith(os.path.normcase(root + os.sep)):
            rel = os.path.relpath(path, root)
            parts = rel.split(os.sep)
            if any(p in ('', '.', '..', '.winbrew-trash') for p in parts):
                break
            cursor = root
            inspect_path(cursor)
            for part in parts:
                cursor = os.path.join(cursor, part)
                inspect_path(cursor)
            origin = unix_root + '/' + '/'.join(parts)
            if len(origin.encode('utf-8')) >= 1024:
                raise ValueError('Path is too long for WinBrew recovery metadata')
            return path, root, origin
    raise ValueError('Recycle is supported only inside WinBrew Documents and Desktop')


def capture(path):
    path, root, origin = approved(path)
    return (path, root, origin, inspect_path(path))


def recycle(item):
    path, root, origin, expected = item
    if approved(path) != (path, root, origin) or inspect_path(path) != expected:
        raise ValueError('The selected item changed; select it again')
    store = os.path.join(root, '.winbrew-trash')
    try:
        os.mkdir(store)
    except FileExistsError:
        pass
    if not stat.S_ISDIR(inspect_path(store)[2]):
        raise ValueError('Recycle Bin is not a directory')
    # A unique container means rename never replaces another recycled file.
    box = os.path.join(store, str(uuid.uuid4().int) + '-0')
    os.mkdir(box)
    leaf = os.path.basename(path)
    marker_name = '.winbrew-original-location-1' if leaf == '.winbrew-original-location' else '.winbrew-original-location'
    marker = os.path.join(box, marker_name)
    moved = False
    try:
        with open(marker, 'x', encoding='utf-8', newline='') as f:
            f.write('WINBREW-ORIGIN-1\n' + origin)
            f.flush()
            os.fsync(f.fileno())
        if inspect_path(path) != expected:
            raise ValueError('The selected item changed; nothing was moved')
        os.rename(path, os.path.join(box, leaf))
        moved = True
    finally:
        if not moved:
            # Only our newly created metadata/empty container may be removed.
            if os.path.isfile(marker):
                os.remove(marker)
            os.rmdir(box)
    return os.path.join(box, leaf)


def install():
    import bpy
    import winreg
    from bpy.app.handlers import persistent

    def apply_view():
        view = bpy.context.preferences.view
        view.filebrowser_display_type = 'SCREEN'
        if hasattr(view, 'preferences_display_type'):
            view.preferences_display_type = 'SCREEN'
        # Saved scenes can contain secondary windows unsupported by this backend.
        # Keep the current working UI, without modifying the loaded scene file.
        bpy.context.preferences.filepaths.use_load_ui = False

    class WINBREW_OT_recycle(bpy.types.Operator):
        bl_idname = 'winbrew.recycle'
        bl_label = 'Move to Recycle Bin'
        bl_description = 'Move selected items to the WinBrew Recycle Bin'

        @classmethod
        def poll(cls, context):
            return context.area is not None and context.area.type == 'FILE_BROWSER'

        def selection(self, context):
            directory = os.fsdecode(context.space_data.params.directory)
            entries = list(context.selected_files)
            if not entries:
                raise ValueError('Select a file or folder first')
            return [capture(os.path.join(directory, f.relative_path)) for f in entries]

        def invoke(self, context, event):
            try:
                self._items = self.selection(context)
            except (OSError, ValueError) as error:
                self.report({'ERROR'}, str(error))
                return {'CANCELLED'}
            return context.window_manager.invoke_confirm(self, event)

        def execute(self, context):
            moved = 0
            try:
                items = self._items if hasattr(self, '_items') else self.selection(context)
                # Validate every selected identity before the first move.
                for item in items:
                    if capture(item[0]) != item:
                        raise ValueError('Selection changed; select the items again')
                for item in items:
                    recycle(item)
                    moved += 1
            except (OSError, ValueError) as error:
                self.report({'ERROR'}, f'{moved} item(s) moved to Recycle Bin; {error}')
                bpy.ops.file.refresh()
                return {'CANCELLED'}
            bpy.ops.file.refresh()
            self.report({'INFO'}, f'Moved {moved} item(s) to WinBrew Recycle Bin')
            return {'FINISHED'}

    def redirect_code(code):
        constants = tuple(redirect_code(c) if isinstance(c, types.CodeType) else
                          'winbrew.recycle' if c == 'file.delete' else c for c in code.co_consts)
        return code.replace(co_consts=constants)

    def redirect_actions():
        # Preserve Blender's own menus; replace just the unsupported delete action.
        for name in dir(bpy.types):
            if name.startswith('FILEBROWSER_MT_'):
                draw = getattr(getattr(bpy.types, name), 'draw', None)
                if hasattr(draw, '__code__'):
                    draw.__code__ = redirect_code(draw.__code__)
        wm = bpy.context.window_manager
        for config in (wm.keyconfigs.default, wm.keyconfigs.user, wm.keyconfigs.addon):
            if config:
                for keymap in config.keymaps:
                    if keymap.space_type == 'FILE_BROWSER':
                        for item in keymap.keymap_items:
                            if item.idname == 'FILE_OT_delete':
                                item.idname = 'winbrew.recycle'
                            elif item.idname == 'file.delete':
                                item.idname = 'winbrew.recycle'

    @persistent
    def after_load(_):
        apply_view()
        redirect_actions()

    apply_view()
    for subkey in (r'Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders',
                   r'Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders'):
        with winreg.CreateKey(winreg.HKEY_CURRENT_USER, subkey) as key:
            for name, folder in (('Personal', 'Documents'), ('Desktop', 'Desktop')):
                winreg.SetValueEx(key, name, 0, winreg.REG_SZ,
                                 'Z:\\data\\WinBrew\\Users\\Default\\' + folder)
    bpy.utils.register_class(WINBREW_OT_recycle)
    bpy.app.handlers.load_post.append(after_load)
    redirect_actions()
    print('WINBREW: inline editors, preserved working UI, recoverable file-browser deletion installed')


if __name__ == '__main__':
    install()
