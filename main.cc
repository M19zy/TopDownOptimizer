#include "GenericJoin.h"
#include "LoadFile.h"
#include "Optimizer.h"
#include "Timer.h"

#include <assert.h>
#include <iostream>
#include <string>

namespace
{


void PrintRelation(const Relation& relation)
{
    const auto& attrs = relation.Attrs();
    size_t tupleNum = relation.Length();

    std::cout << "Relation name: " << relation.Name() << std::endl;

    for (const auto& attr : attrs)
    {
        std::cout << attr.first << " ";
    }
    std::cout << std::endl;

    for(size_t tupleIndex = 0; tupleIndex < tupleNum; tupleIndex++)
    {
        for (const  auto& attr : attrs)
        {
            std::cout << (attr.second)[tupleIndex] << " ";
        }
        std::cout << std::endl;
    }
}


}

std::string DatabasePath;
std::string RelationDir;
std::string QueryPath;

int main(int argc, char* argv[])
{

    assert(argc >= 3);

    // if (argv[1] == std::string("-d"))
    // {
    DatabasePath = "data/" + std::string(argv[1]) + "/";
    RelationDir = DatabasePath + "relation/";
    std::cout << "rel path: " << RelationDir << std::endl;
    QueryPath = DatabasePath + "sql/" + std::string(argv[2]);
    std::cout << "query path: " << QueryPath << std::endl;
    // }

    std::string schemaPath = QueryPath;
    Schema schema(schemaPath);
    auto [relationNames, attrNames] = schema.Load();
    // GloablData::GAttributes = attrNames;

    std::vector<Relation> relations(relationNames.size());
    for (size_t i = 0; i < relationNames.size(); i++)
    {
        const std::string& relName = relationNames[i];
        std::string relPath = RelationDir + relName + ".desc";
        RelationDesc desc(relPath);
        relations[i] = desc.Load();
        // GloablData::GRelation.emplace_back(desc.Load());
    }

    // optimize plan
    std::vector<RelationRef> relationRefs;
    for (size_t relIndex = 0; relIndex < relations.size(); relIndex++)
        relationRefs.push_back(relations[relIndex]);
    // for (size_t relIndex = 0; relIndex < GloablData::GRelation.size(); relIndex++)
    //     relationRefs.push_back(GloablData::GRelation[relIndex]);

    Timer tm("timer");
    LTOptimizer* optimizer = new EHLTOptimizer(std::move(relationRefs), attrNames);
    // LTOptimizer* optimizer = new DPLTOptimizer();
    auto attrNum = attrNames.size();
    auto plan = optimizer->operator()();

    auto opTime = tm.Timing();
    std::cout << "GVO: ";
    for (auto v : optimizer->GVO)
        std::cout << v << ' ';
    std::cout << std::endl;

    // sort relation by GVO
    for (auto& rel : relations)
    {
        // std::vector<std::string> sattrs;
        // for (std::string& attr : optimizer->GVO)
        //     if (rel.ExistAttr(attr))
        //         sattrs.push_back(attr);
        rel.Sort(optimizer->GVO);
    }

    std::cout << "Start joining" << std::endl;
    auto stJoin = tm.Timing();

    // calculate
    GenericJoin join(std::move(plan), std::move(relations), std::move(attrNames));
    join();
    // plan->Execute();

    auto edJoin = tm.Timing();

    std::cout << "op time     join time    total time     attr num:" << std::endl;
    std::cout << opTime << "     " << edJoin-stJoin << "     " << edJoin << "     " << attrNum << std::endl;
    // std::cout << "Join result number: " << res.Length() << std::endl;

    return 0;
}