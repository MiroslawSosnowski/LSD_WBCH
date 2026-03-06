#include "query_engine.h"
#include <QStringList>

QueryEngine::QueryEngine(const ModelDatabase &db) : db(db) {}

// ── Entity lookups ─────────────────────────────────────────────────────────────
const NodeMeta     *QueryEngine::node(int id)     const
{ auto it = db.node_index.find(id);     return it != db.node_index.end()     ? &it->second : nullptr; }
const ElementMeta  *QueryEngine::element(int id)  const
{ auto it = db.element_index.find(id);  return it != db.element_index.end()  ? &it->second : nullptr; }
const PartMeta     *QueryEngine::part(int id)     const
{ auto it = db.part_index.find(id);     return it != db.part_index.end()     ? &it->second : nullptr; }
const MaterialMeta *QueryEngine::material(int id) const
{ auto it = db.material_index.find(id); return it != db.material_index.end() ? &it->second : nullptr; }
const SectionMeta  *QueryEngine::section(int id)  const
{ auto it = db.section_index.find(id);  return it != db.section_index.end()  ? &it->second : nullptr; }
const SetMeta      *QueryEngine::set(int id)       const
{ auto it = db.set_index.find(id);      return it != db.set_index.end()      ? &it->second : nullptr; }

// ── Graph traversal ───────────────────────────────────────────────────────────
PartGraph QueryEngine::getPartGraph(int partId) const
{
    PartGraph g;
    g.part = part(partId);
    if (!g.part) return g;

    auto matIt = db.part_to_material.find(partId);
    if (matIt != db.part_to_material.end())
        g.material = material(matIt->second);

    auto secIt = db.part_to_section.find(partId);
    if (secIt != db.part_to_section.end())
        g.section = section(secIt->second);

    auto eleIt = db.part_to_elements.find(partId);
    if (eleIt != db.part_to_elements.end())
        for (int eid : eleIt->second)
            if (const ElementMeta *e = element(eid))
                g.elements.push_back(e);

    return g;
}

ElementGraph QueryEngine::getElementGraph(int elemId) const
{
    ElementGraph g;
    g.element = element(elemId);
    if (!g.element) return g;

    g.part = part(g.element->partId);

    auto nit = db.element_to_nodes.find(elemId);
    if (nit != db.element_to_nodes.end())
        for (int nid : nit->second)
            if (const NodeMeta *n = node(nid))
                g.nodes.push_back(n);

    return g;
}

MaterialGraph QueryEngine::getMaterialGraph(int matId) const
{
    MaterialGraph g;
    g.material = material(matId);
    if (!g.material) return g;

    g.hasEos     = g.material->hasEos;
    g.hasThermal = g.material->hasThermal;

    for (const auto &[pid, mid] : db.part_to_material)
        if (mid == matId)
            if (const PartMeta *p = part(pid))
                g.usedByParts.push_back(p);

    return g;
}

// ── Collections ───────────────────────────────────────────────────────────────
QVector<int> QueryEngine::allPartIds() const
{
    QVector<int> v; v.reserve(db.part_index.size());
    for (const auto &kv : db.part_index) v.append(kv.first);
    return v;
}
QVector<int> QueryEngine::allMaterialIds() const
{
    QVector<int> v; v.reserve(db.material_index.size());
    for (const auto &kv : db.material_index) v.append(kv.first);
    return v;
}
QVector<int> QueryEngine::allElementIds() const
{
    QVector<int> v; v.reserve(db.element_index.size());
    for (const auto &kv : db.element_index) v.append(kv.first);
    return v;
}
QVector<int> QueryEngine::allNodeIds() const
{
    QVector<int> v; v.reserve(db.node_index.size());
    for (const auto &kv : db.node_index) v.append(kv.first);
    return v;
}

QVector<int> QueryEngine::elementsOfPart(int partId) const
{
    auto it = db.part_to_elements.find(partId);
    if (it == db.part_to_elements.end()) return {};
    return QVector<int>(it->second.begin(), it->second.end());
}

QVector<int> QueryEngine::partsUsingMaterial(int matId) const
{
    QVector<int> v;
    for (const auto &[pid, mid] : db.part_to_material)
        if (mid == matId) v.append(pid);
    return v;
}

QVector<const BlockEntry *> QueryEngine::blocksOfType(BlockType t) const
{
    QVector<const BlockEntry *> v;
    for (const auto &b : db.blocks)
        if (b.type == t) v.append(&b);
    return v;
}

// ── Search ────────────────────────────────────────────────────────────────────
QVector<SearchResult> QueryEngine::searchByKeyword(const QString &prefix) const
{
    QVector<SearchResult> results;
    const QString upper = prefix.toUpper();

    for (const auto &b : db.blocks)
    {
        const QString kw = QString::fromStdString(b.keyword).toUpper();
        if (kw.contains(upper))
        {
            SearchResult r;
            r.kind     = SearchResult::Kind::BLOCK;
            r.keyword  = QString::fromStdString(b.keyword);
            r.location = { b.fileId, b.startLine, b.startByte };
            r.label    = r.keyword;
            results.append(r);
        }
    }
    return results;
}

QVector<SearchResult> QueryEngine::searchById(int id) const
{
    QVector<SearchResult> results;

    if (const NodeMeta *n = node(id)) {
        SearchResult r;
        r.kind = SearchResult::Kind::NODE; r.id = id;
        r.location = n->source;
        r.label = QString("NODE %1  (%2, %3, %4)")
                      .arg(id)
                      .arg(n->x, 0, 'f', 3)
                      .arg(n->y, 0, 'f', 3)
                      .arg(n->z, 0, 'f', 3);
        results.append(r);
    }
    if (const ElementMeta *e = element(id)) {
        SearchResult r;
        r.kind = SearchResult::Kind::ELEMENT; r.id = id;
        r.location = e->source;
        r.label = QString("ELEMENT %1  part=%2").arg(id).arg(e->partId);
        results.append(r);
    }
    if (const PartMeta *p = part(id)) {
        SearchResult r;
        r.kind = SearchResult::Kind::PART; r.id = id;
        r.location = p->source;
        r.label = QString("PART %1  \"%2\"").arg(id)
                      .arg(QString::fromStdString(p->name));
        results.append(r);
    }
    if (const MaterialMeta *m = material(id)) {
        SearchResult r;
        r.kind = SearchResult::Kind::MATERIAL; r.id = id;
        r.location = m->source;
        r.label = QString("*%1  ID=%2").arg(QString::fromStdString(m->keyword)).arg(id);
        results.append(r);
    }
    return results;
}

// ── Summary text ──────────────────────────────────────────────────────────────
QString QueryEngine::summaryText() const
{
    QStringList lines;
    lines << QString("Nodes:     %1").arg(db.node_index.size());
    lines << QString("Elements:  %1").arg(db.element_index.size());
    lines << QString("Parts:     %1").arg(db.part_index.size());
    lines << QString("Materials: %1").arg(db.material_index.size());
    lines << QString("Sections:  %1").arg(db.section_index.size());
    lines << QString("Contacts:  %1").arg(db.contacts.size());
    lines << QString("Sets:      %1").arg(db.set_index.size());
    lines << QString("Files:     %1").arg(db.files.size());
    return lines.join('\n');
}
