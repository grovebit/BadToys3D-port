"""Exercise the build wrapper's CLI and file handling without a compiler or a GUI."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "build.sh"


class BuildTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bt3d setup ")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.repo = self.base / "project with spaces"
        self.repo.mkdir()
        shutil.copy2(SCRIPT, self.repo / "build.sh")
        self.bin = self.base / "bin"
        self.bin.mkdir()
        self.log = self.base / "commands"
        self.env = {**os.environ, "PATH": f"{self.bin}:{os.environ['PATH']}",
                    "SETUP_TEST_LOG": str(self.log)}
        for key in ("DATA_PCK_SRC", "MIDI_SRC"):
            self.env.pop(key, None)
        for tool in ("git", "cc", "c++", "make", "xcode-select"):
            self.stub(tool, "exit 0\n")
        self.stub("uname", "echo Linux\n")
        self.stub("cmake", '''
printf '%s\\n' "$*" >> "$SETUP_TEST_LOG"
if [ "${1:-}" = --build ]; then
    mkdir -p "$2"
    printf '#!/bin/sh\\necho launched >> "$SETUP_TEST_LOG"\\n' > "$2/bt3d_raylib"
    chmod +x "$2/bt3d_raylib"
fi
''')

    def stub(self, name, body):
        path = self.bin / name
        path.write_text("#!/bin/sh\nset -eu\n" + body)
        path.chmod(0o755)

    def run_build(self, *args):
        return subprocess.run(["bash", str(self.repo / "build.sh"), *args],
                              cwd=self.base, env=self.env,
                              capture_output=True, text=True)

    def pack(self, path=None):
        path = path or self.repo / "data.pck"
        path.write_bytes(b"test pack contents")
        return path

    def test_help_needs_no_data_or_build(self):
        result = self.run_build("--help")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--run", result.stdout)
        self.assertFalse(self.log.exists())

    def test_missing_data_stops_before_build(self):
        result = self.run_build("--run")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Game data not found", result.stderr)
        self.assertFalse(self.log.exists())

    def test_explicit_relative_path_with_spaces_and_launch(self):
        pack = self.pack(self.base / "original pack.pck")
        result = self.run_build("--data", pack.name, "--run")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.repo / "build/play/data.pck").read_bytes(), pack.read_bytes())
        self.assertIn("launched", self.log.read_text())

    def test_no_run_and_rerun_preserve_saves(self):
        pack = self.pack()
        result = self.run_build("--native")
        self.assertEqual(result.returncode, 0, result.stderr)
        pack.unlink()
        save = self.repo / "build/play/savegame.dat"
        save.write_bytes(b"existing save")
        result = self.run_build("--native")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(save.read_bytes(), b"existing save")
        self.assertNotIn("launched", self.log.read_text())

    def test_invalid_explicit_pack_does_not_fall_back(self):
        self.pack()
        result = self.run_build("--data", "missing.pck")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Cannot read data pack", result.stderr)
        self.assertFalse(self.log.exists())

    def test_bad_arguments_do_not_build(self):
        for args in (("--data",), ("--surprise",)):
            result = self.run_build(*args)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(self.log.exists())

    def test_configure_failure_does_not_build_or_launch(self):
        self.pack()
        self.stub("cmake", 'echo configure >> "$SETUP_TEST_LOG"\nexit 1\n')
        result = self.run_build()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Configure failed", result.stderr)
        self.assertEqual(self.log.read_text(), "configure\n")
        self.assertFalse((self.repo / "build/play/data.pck").exists())

    def test_unsupported_platform_has_guidance(self):
        self.stub("uname", "echo MINGW64_NT\n")
        result = self.run_build()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Windows", result.stderr)
        self.assertFalse(self.log.exists())

    def test_missing_tool_has_guidance(self):
        (self.bin / "cmake").unlink()
        (self.bin / "dirname").symlink_to(shutil.which("dirname"))
        self.env["PATH"] = str(self.bin)
        result = subprocess.run(["/bin/bash", str(self.repo / "build.sh")],
                                env=self.env, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing prerequisite: cmake", result.stderr)
        self.assertFalse(self.log.exists())

    def test_cli_data_overrides_environment_and_auto_detection(self):
        self.pack()
        self.env["DATA_PCK_SRC"] = "does-not-exist.pck"
        explicit = self.base / "chosen.pck"
        explicit.write_bytes(b"explicit pack")
        result = self.run_build("--data", str(explicit), "--native")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.repo / "build/play/data.pck").read_bytes(), b"explicit pack")

    def test_default_build_allows_missing_data_and_does_not_launch(self):
        result = self.run_build()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("No data.pck found", result.stdout)
        self.assertTrue((self.repo / "build/play/bt3d_raylib").exists())
        self.assertNotIn("launched", self.log.read_text())

    def test_run_rejects_cross_platform_and_maintenance_modes(self):
        for option in ("--win", "--web", "--switch", "--all", "--clean", "--stage-release", "--docker-image"):
            result = self.run_build(option, "--run")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("--run requires", result.stderr)
        self.assertFalse(self.log.exists())

    def test_windows_and_web_receive_discovered_data(self):
        pack = self.pack()
        self.stub("docker", "exit 0\n")
        scripts = self.repo / "scripts"
        scripts.mkdir()
        (scripts / "build-docker.sh").write_text(
            'printf "%s\\n" "$DATA_PCK_SRC" >> "$SETUP_TEST_LOG"\n'
            'mkdir -p build/windows\ntouch build/windows/bt3d_raylib.exe\n')
        (scripts / "build-web-docker.sh").write_text(
            'printf "%s\\n" "$DATA_PCK_SRC" >> "$SETUP_TEST_LOG"\n')
        result = self.run_build("--win", "--web")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.repo / "build/windows/data.pck").read_bytes(), pack.read_bytes())
        self.assertEqual(self.log.read_text().splitlines(), [str(pack), str(pack)])

    def test_docker_not_running_stops_before_build(self):
        self.stub("docker", "exit 1\n")
        result = self.run_build("--web")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Docker is not ready", result.stderr)
        self.assertFalse(self.log.exists())

    def test_all_requires_macos_before_building(self):
        result = self.run_build("--all")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("macOS .app builds require macOS", result.stderr)
        self.assertFalse(self.log.exists())

    def test_clean_preserves_native_saves(self):
        directory = self.repo / "build/play"
        directory.mkdir(parents=True)
        save = directory / "savegame.dat"
        save.write_bytes(b"keep")
        result = self.run_build("--clean")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(save.read_bytes(), b"keep")
        self.assertFalse(self.log.exists())

    def test_macos_packages_data_and_launches_app(self):
        self.stub("uname", "echo Darwin\n")
        pack = self.pack()
        scripts = self.repo / "scripts"
        scripts.mkdir()
        shutil.copy2(SCRIPT.parent / "scripts/package-mac-app.sh", scripts)
        result = self.run_build("--macos", "--run")
        self.assertEqual(result.returncode, 0, result.stderr)
        app = self.repo / "dist/macos/Bad Toys 3D.app"
        self.assertTrue((app / "Contents/MacOS/bt3d_raylib").exists())
        self.assertEqual((app.parent / "data.pck").read_bytes(), pack.read_bytes())
        self.assertIn("launched", self.log.read_text())

    def test_macos_checks_developer_tools(self):
        self.stub("uname", "echo Darwin\n")
        self.stub("xcode-select", "exit 1\n")
        result = self.run_build()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("xcode-select --install", result.stderr)
        self.assertFalse(self.log.exists())


if __name__ == "__main__":
    unittest.main()
