#include "model_database.h"
#include <QFileInfo>

// ─────────────────────────────────────────────────────────────────────────────
int ModelDatabase::registerFile(const QString &absPath, int parentId, int includeLine)
{
    // Dedup by path
    for (const auto &f : files)
        if (f.path == absPath.toStdString())
            return f.id;

    FileEntry fe;
    fe.id          = files.size();
    fe.path        = absPath.toStdString();
    fe.name        = QFileInfo(absPath).fileName().toStdString();
    fe.size        = static_cast<size_t>(QFileInfo(absPath).size());
    fe.parentId    = parentId;
    fe.includeLine = includeLine;
    files.append(fe);
    return fe.id;
}

const FileEntry *ModelDatabase::fileById(int id) const
{
    if (id < 0 || id >= files.size())
        return nullptr;
    return &files[id];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Build Cross-Reference Graph
//  Called once after all blocks have been demand-parsed.
// ─────────────────────────────────────────────────────────────────────────────
void ModelDatabase::buildCrossReferenceGraph()
{
    part_to_elements.clear();
    part_to_material.clear();
    part_to_section.clear();
    element_to_nodes.clear();
    material_to_eos.clear();
    material_to_thermal.clear();

    // PART → MATERIAL, PART → SECTION
    for (const auto &[pid, part] : part_index)
    {
        if (part.matId     != 0) part_to_material[pid] = part.matId;
        if (part.sectionId != 0) part_to_section[pid]  = part.sectionId;
    }

    // ELEMENT → PART, ELEMENT → NODES
    for (const auto &[eid, elem] : element_index)
    {
        part_to_elements[elem.partId].push_back(eid);

        if (!elem.nodeIds.empty())
            element_to_nodes[eid] = elem.nodeIds;
    }

    // MATERIAL → EOS / THERMAL
    for (const auto &[mid, mat] : material_index)
    {
        material_to_eos[mid]     = mat.hasEos     ? mid : -1;
        material_to_thermal[mid] = mat.hasThermal ? mid : -1;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void ModelDatabase::clear()
{
    files.clear();
    node_index.clear();
    element_index.clear();
    part_index.clear();
    material_index.clear();
    section_index.clear();
    set_index.clear();
    contacts.clear();
    blocks.clear();
    part_to_elements.clear();
    part_to_material.clear();
    part_to_section.clear();
    element_to_nodes.clear();
    material_to_eos.clear();
    material_to_thermal.clear();
}
