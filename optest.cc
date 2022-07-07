#include <iostream>
#include <fstream>
#include <sstream>
#include <tuple>
#include <vector>
#include <filesystem>
#include <string>
#include "Optimizer.h"
#include "Timer.h"


std::tuple<int, int, double> test(std::vector<RelationRef>&& relations, std::vector<std::string>& attrs)
{
    int relNum = relations.size();
    int attrNum = attrs.size();
    if (attrNum > 20)
    {
        return std::make_tuple(relNum, attrNum, 1000);
    }

    //  timing 
    Timer timer("start timing");

    LTOptimizer* optimizer = new DPLTOptimizer(std::move(relations), attrs);
    auto plan = (*optimizer)();
    delete optimizer;

    auto end = timer.Timing();

    return std::make_tuple(relNum, attrNum, end);
}


void testSQL(std::ofstream& outfile, const std::string& sqlFile)
{
    std::vector<Relation> relations;
    std::vector<std::string> attrs;

    // open sql file
    {
        /*
        *  format:
        *    attr1 attr2 ...    // first line lists all attributes
        *    relName tupleNum attr1 attr2 ...
        */
       std::ifstream sqlfile(sqlFile, std::ios::in);
       std::string line;

       {
           // process first line
           std::getline(sqlfile, line);

           std::stringstream lineStream(line);
           std::string attr;

           while ( lineStream >> attr )
            attrs.push_back(attr);
       }

       while ( std::getline(sqlfile, line) )
       {
           std::string relName;
           std::string attrName;
           int tupleNum;

           std::stringstream lineStream(line);

           lineStream >> relName >> tupleNum;
            Relation relation;
            relation.SetName(relName);
            relation.SetTupleNum(tupleNum);

            while ( lineStream >> attrName )
            {
                relation.Insert(attrName, std::vector<int>{});
            }
            
            relations.emplace_back(std::move(relation));
       }
    }

    std::vector<RelationRef> relationRefs;
    for (auto& rel : relations)
    {
        relationRefs.push_back(rel);
    }

    auto&& [relnum, attrnum, cost_time] = test(std::move(relationRefs), attrs);

    outfile << relnum << " " << attrnum << " " << cost_time << std::endl;
}


int main()
{
    // open sql 

    std::ofstream outfile("analysis/largeSQLBench-dp.txt", std::ios::out | std::ios::trunc);

    std::string path = "./largeSQL";
    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        testSQL(outfile, entry.path().string());
    }

    outfile.close();

    return 0;
}