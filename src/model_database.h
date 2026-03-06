#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  model_database.h  –  Layer 2: Model Database + Cross-Reference Graph
//
//  Central store.  All parsers write here; all UI reads from here.
//
//  Cross-Reference Graph (adjacency lists):
//
//    PART ──► ELEMENTS  (part_to_elements)
//         ──► MATERIAL  (part_to_material)
//         ──► SECTION   (part_to_section)
//    ELEMENT ──► NODES  (element_to_nodes)
//    MATERIAL ──► EOS   (material_to_eos)
//             ──► THERM (material_to_thermal)
// ═════════════════════════════════════════════════════════════════════════════
#include "model_index.h"
#include <unordered_map>
#include <vector>
#include <QString>
#include <QVector>

class ModelDatabase
{
public:
    // ── File registry ─────────────────────────────────────────────────────────
    QVector<FileEntry>  files;        // indexed by FileEntry::id
    int registerFile(const QString &absPath, int parentId = -1, int includeLine = 0);
    const FileEntry *fileById(int id) const;

    // ── Layer 2 indices ───────────────────────────────────────────────────────
    std::unordered_map<int, NodeMeta>     node_index;
    std::unordered_map<int, ElementMeta>  element_index;
    std::unordered_map<int, PartMeta>     part_index;
    std::unordered_map<int, MaterialMeta> material_index;
    std::unordered_map<int, SectionMeta>  section_index;
    std::unordered_map<int, SetMeta>      set_index;
    std::vector<ContactMeta>              contacts;

    // ── Block index (Layer 3 output) ──────────────────────────────────────────
    std::vector<BlockEntry>               blocks;

    // ── Cross-Reference Graph ─────────────────────────────────────────────────
    std::unordered_map<int, std::vector<int>> part_to_elements;
    std::unordered_map<int, int>              part_to_material;   // part→mat id
    std::unordered_map<int, int>              part_to_section;    // part→sec id
    std::unordered_map<int, std::vector<int>> element_to_nodes;   // elem→node ids
    std::unordered_map<int, int>              material_to_eos;    // mat→eos id (-1=none)
    std::unordered_map<int, int>              material_to_thermal; // mat→thermal mat id

    // ── Build / clear ─────────────────────────────────────────────────────────
    void buildCrossReferenceGraph();
    void clear();

    // ── Stats ─────────────────────────────────────────────────────────────────
    int nodeCount()     const { return static_cast<int>(node_index.size()); }
    int elementCount()  const { return static_cast<int>(element_index.size()); }
    int partCount()     const { return static_cast<int>(part_index.size()); }
    int materialCount() const { return static_cast<int>(material_index.size()); }
};
