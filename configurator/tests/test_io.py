"""Storage and backup IO round-trips."""

import os

from keybchord_configurator.io.backup import backup_to_zip, restore_from_zip
from keybchord_configurator.io.storage import default_tree, load_tree, save_tree


def test_volume_name_match_is_case_insensitive():
    from keybchord_configurator.io import device
    assert device._matches_volume("KEYBCHORD")
    assert device._matches_volume("KeybChord")
    assert device._matches_volume("keybchord")
    assert device._matches_volume("  keybchord  ")
    assert not device._matches_volume("Other")
    assert not device._matches_volume("")


def test_eject_linux_powers_off_after_unmount(monkeypatch):
    from keybchord_configurator.io import device
    monkeypatch.setattr(device, "_IS_WINDOWS", False)
    monkeypatch.setattr(device, "_IS_MAC", False)
    monkeypatch.setattr(os.path, "exists", lambda _path: True)
    monkeypatch.setattr(device, "_linux_block_device",
                        lambda _path: "/dev/sdx")
    calls = []

    def fake_run(command):
        calls.append(command)
        return True, "ok"

    monkeypatch.setattr(device, "_run_eject", fake_run)
    ok, _message = device.eject("/media/user/KeybChord")
    assert ok
    assert calls == [
        ["udisksctl", "unmount", "-b", "/dev/sdx"],
        ["udisksctl", "power-off", "-b", "/dev/sdx"],
    ]


def test_device_display_path_windows_drive(monkeypatch):
    from keybchord_configurator.io import device
    monkeypatch.setattr(device, "_IS_WINDOWS", True)
    assert device.display_path("D:\\") == "D:"


def test_device_display_path_posix_mount(monkeypatch):
    from keybchord_configurator.io import device
    monkeypatch.setattr(device, "_IS_WINDOWS", False)
    assert device.display_path("/media/user/KeybChord/") == "/media/user/KeybChord"


def test_find_device_windows_uppercase_label(monkeypatch):
    from keybchord_configurator.io import device
    monkeypatch.setattr(device, "_IS_WINDOWS", True)
    monkeypatch.setattr(device, "_IS_MAC", False)
    monkeypatch.setattr(os.path, "exists", lambda p: p == "D:\\")
    monkeypatch.setattr(
        device, "_windows_volume_label",
        lambda letter: "KEYBCHORD" if letter == "D" else None)
    assert device.find_device() == "D:\\"


def test_find_device_linux_case_insensitive(monkeypatch, tmp_path):
    from keybchord_configurator.io import device
    monkeypatch.setattr(device, "_IS_WINDOWS", False)
    monkeypatch.setattr(device, "_IS_MAC", False)
    root = str(tmp_path)

    def fake_listdir(path):
        if path == "/media":
            return ["KEYBCHORD", "usbstick"]
        return []

    def fake_isdir(path):
        return path == f"/media/KEYBCHORD"

    monkeypatch.setattr(os, "listdir", fake_listdir)
    monkeypatch.setattr(os.path, "isdir", fake_isdir)
    assert device.find_device() == "/media/KEYBCHORD"


def test_save_and_load_roundtrip(tmp_path):
    tree = default_tree()
    save_tree(tree, tmp_path)
    loaded = load_tree(tmp_path)
    assert len(loaded.rhythms) == 12
    assert len(loaded.bass) == 9
    assert len(loaded.banks) == 10
    assert all(len(bank) == 8 for bank in loaded.banks)
    assert loaded.config.to_dict() == tree.config.to_dict()
    assert loaded.keymap.to_dict() == tree.keymap.to_dict()


def test_rhythm_name_list_order(tmp_path):
    tree = default_tree()
    # Add a user rhythm; it should come after the built-ins.
    from keybchord_configurator.models.rhythm import RhythmPattern
    tree.rhythms.append(("zzz.json", RhythmPattern.from_dict(
        {"name": "Zebra", "steps_per_bar": 16, "tracks": [
            {"note": 36, "name": "kick", "pattern": [1] * 16}]})))
    save_tree(tree, tmp_path)
    loaded = load_tree(tmp_path)
    names = loaded.rhythm_names()
    assert names[0] == "Rock 1"
    assert names[-1] == "Zebra"


def test_backup_and_restore(tmp_path):
    tree = default_tree()
    save_tree(tree, tmp_path)

    zip_path = tmp_path / "backup.zip"
    archived = backup_to_zip(tmp_path, zip_path)
    assert len(archived) >= 22  # config + keymap + 10 banks + 12 rhythms + 9 bass

    restore_dir = tmp_path / "restored"
    restore_dir.mkdir()
    restored = restore_from_zip(zip_path, restore_dir)
    assert len(restored) == len(archived)

    loaded = load_tree(restore_dir)
    assert len(loaded.rhythms) == 12
    assert len(loaded.bass) == 9


def test_restore_rejects_foreign_files(tmp_path):
    zip_path = tmp_path / "evil.zip"
    import zipfile
    with zipfile.ZipFile(zip_path, "w") as zf:
        zf.writestr("../evil.txt", "bad")
        zf.writestr("config.json", '{"midi": {}}')
    restore_dir = tmp_path / "r"
    restore_dir.mkdir()
    restored = restore_from_zip(zip_path, restore_dir)
    assert restored == ["config.json"]
    assert not (restore_dir / "evil.txt").exists()


def test_builtin_jdxi_libraries_are_valid():
    from keybchord_configurator.io.libraries import _load_file, builtin_library_dir

    paths = sorted(builtin_library_dir().glob("roland-jd-xi-*.json"))
    assert len(paths) == 4
    libraries = {}
    for path in paths:
        library, error = _load_file(path)
        assert error is None, error
        assert library is not None
        libraries[library.name] = library

    assert set(libraries) == {
        "Roland JD-Xi Programs",
        "Roland JD-Xi Extra Programs",
        "Roland JD-Xi Tones",
        "Roland JD-Xi Controls",
    }

    programs = libraries["Roland JD-Xi Programs"]
    assert programs.command_count() == 512
    a01 = programs.groups[0].commands[0]
    assert a01.name == "A01"
    assert [msg.encoded() for msg in a01.messages] == [
        [0xB0, 0, 85], [0xB0, 32, 64], [0xC0, 0]]
    assert [msg.delay_ms for msg in a01.messages] == [0, 25, 25]
    assert [cmd.bytes for cmd in a01.macro_commands()] == [
        [0xB0, 0, 85], [0xB0, 32, 64], [0xC0, 0]]

    e01 = programs.groups[4].commands[0]
    assert e01.name == "E01"
    assert [msg.encoded() for msg in e01.messages] == [
        [0xB0, 0, 85], [0xB0, 32, 0], [0xC0, 0]]
    assert [msg.delay_ms for msg in e01.messages] == [0, 25, 25]

    assert libraries["Roland JD-Xi Extra Programs"].command_count() == 512
    assert libraries["Roland JD-Xi Tones"].command_count() == 353
    assert libraries["Roland JD-Xi Controls"].command_count() == 102
