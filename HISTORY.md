# History

All notable changes to this project will be documented in this file.

The format is inspired by [Keep a Changelog](https://keepachangelog.com/),  
and this project adheres to [Semantic Versioning](https://semver.org/).

---

## [Unreleased]

- Add support for additional mtools-like utilities (`minfo`, `mcp`, `mdel`)  
- FAT32 root directory traversal (currently only FAT12/16 supported)  
- Long File Name (LFN) handling (currently skipped)  
- More robust error messages and validation  

---

## [0.0.2] – 2025-08-22

### Added
- `--version` / `-V` command line option to `mdir` showing version and license info.  
- `HISTORY.md` and `CHANGELOG` style documentation.  

### Changed
- Makefile reworked to build sources from `src/` directory.  
- Removed generation of `.d` dependency files to simplify builds.  
- Updated install/uninstall targets to work cleanly under Windows (Cygwin/MinGW) and POSIX.  

---

## [0.0.1] – 2025-08-20

### Added
- Initial minimal `mdir` utility:  
  - Lists FAT12/FAT16 root directory entries.  
  - Displays 8.3 filenames, size, DOS date/time, and attributes.  
  - Skips deleted and LFN entries.  
- Basic `Makefile` to build `mformat`, `mdir`, `minfo`, and `mcp` utilities.  

---

## License

This project is licensed under the **GNU GPL v3 or later**.  
See the [LICENSE](LICENSE) file for details.
