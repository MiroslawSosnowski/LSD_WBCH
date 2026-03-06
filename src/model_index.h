#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  model_index.h
//  Shared data structures for all five layers of the LS-DYNA Workbench.
//  Keep this header lean – no Qt, no STL containers in hot paths.
// ═════════════════════════════════════════════════════════════════════════════
#include <string>
#include <vector>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
//  Layer 1 – File registry
// ─────────────────────────────────────────────────────────────────────────────
struct FileEntry {
    int         id       = 0;
    std::string path;
    std::string name;           // basename only
    size_t      size     = 0;   // bytes
    int         parentId = -1;  // -1 = root, otherwise = including file id
    int         includeLine = 0;// line in parent that has *INCLUDE
};

// ─────────────────────────────────────────────────────────────────────────────
//  Source location  (stable across re-parses)
// ─────────────────────────────────────────────────────────────────────────────
struct LineRef {
    int    fileId     = 0;
    int    lineNumber = 0;      // 1-based
    size_t byteOffset = 0;      // offset into mmap'd buffer
};

// ─────────────────────────────────────────────────────────────────────────────
//  Block descriptor  (Layer 3 – incremental index built first-pass)
// ─────────────────────────────────────────────────────────────────────────────
enum class BlockType : uint8_t {
    UNKNOWN = 0,
    NODE, ELEMENT, PART, MAT, SECTION,
    CONTACT, SET, EOS, DEFINE,
    CONTROL, DATABASE, BOUNDARY, LOAD, INITIAL,
    INCLUDE, INCLUDE_TRANSFORM,
    KEYWORD, END
};

struct BlockEntry {
    BlockType   type      = BlockType::UNKNOWN;
    std::string keyword;        // full keyword e.g. "*ELEMENT_SHELL"
    std::string subtype;        // e.g. "SHELL" / "024"
    int         fileId    = 0;
    int         startLine = 0;  // line of the keyword itself
    int         endLine   = 0;  // last data line (exclusive)
    size_t      startByte = 0;
    size_t      endByte   = 0;
    bool        parsed    = false; // demand-parse flag
};

// ─────────────────────────────────────────────────────────────────────────────
//  Layer 2 – Entity metadata (stored in unordered_maps, keyed by int id)
// ─────────────────────────────────────────────────────────────────────────────

struct NodeMeta {
    int     id = 0;
    double  x = 0, y = 0, z = 0;
    LineRef source;
};

enum class ElemType : uint8_t {
    UNKNOWN = 0, SHELL, SOLID, BEAM, DISCRETE, TSHELL
};

struct ElementMeta {
    int              id     = 0;
    int              partId = 0;
    ElemType         type   = ElemType::UNKNOWN;
    LineRef          source;
    std::vector<int> nodeIds;   // filled on demand-parse
};

struct PartMeta {
    int         id        = 0;
    int         sectionId = 0;
    int         matId     = 0;
    int         eosId     = 0;
    int         hgId      = 0;
    std::string name;
    LineRef     source;
};

struct MaterialMeta {
    int         id      = 0;
    std::string keyword;        // e.g. "MAT_024"
    std::string name;           // human-readable from materials_db.json
    LineRef     source;
    bool        hasEos     = false;
    bool        hasThermal = false;
};

struct SectionMeta {
    int         id   = 0;
    std::string type;           // SHELL / SOLID / BEAM …
    LineRef     source;
};

struct SetMeta {
    int              id   = 0;
    std::string      type;      // PART / NODE / SHELL / SEGMENT …
    LineRef          source;
    std::vector<int> members;   // filled on demand-parse
};

struct ContactMeta {
    std::string type;           // AUTOMATIC_SURFACE_TO_SURFACE …
    int         ssid = 0, msid = 0;
    LineRef     source;
};


