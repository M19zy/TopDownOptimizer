#include "LoadFile.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>


std::pair<std::vector<std::string>, std::vector<std::string>> Schema::Load()
{
    std::vector<std::string> relations;
    std::vector<std::string> attrs;

    std::cout << "Loading schema " << mPath << "..." << std::endl;

    std::ifstream fileData(mPath, std::ios::in);

    {
        std::string line;
        std::getline(fileData, line);
        std::stringstream linesStream(line);

        std::string relation;
        while (linesStream >> relation)
            relations.push_back(relation);
    }

    {
        std::string line;
        std::getline(fileData, line);
        std::stringstream linesStream(line);

        std::string relation;
        while (linesStream >> relation)
            attrs.push_back(relation);
    }

    std::cout << "Schema loaded." << std::endl;

    return std::make_pair(relations, attrs);
}

Relation RelationDesc::Load()
{
    std::ifstream dataFile;
    dataFile.open(mPath, std::ios::in);

    if (!dataFile.is_open())
    {
        throw std::runtime_error(std::string("Failed to open desc file: ") + mPath);
    }

    std::string relationName;
    size_t tupleNum;
    size_t attrNum;
    dataFile >> relationName >> tupleNum >> attrNum;

    std::vector<std::string> attrs(attrNum);
    for (size_t i = 0; i < attrNum; i++)
    {
        std::string attr;
        dataFile >> attr;
        attrs[i] = attr;
    }

    // load relation
    std::string relationFile = mPath.substr(0, mPath.find_last_of("/\\") + 1) + relationName;
    RawRelation rawRelation(relationFile, attrs, tupleNum);
    return rawRelation.Load();
}


Relation RawRelation::Load()
{
    std::cout << "Loading " << mPath;
    std::cout << "   [ Tuple number: " << mTupleNum;
    std::cout << ", Attributes: "; 
    for (auto& attr : mAttrs)
        std::cout << attr << " ";
    std::cout << " ]" << std::endl;


    Relation relation;
    relation.SetName(mPath.substr(1 + mPath.find_last_of("/\\")));
    relation.SetTupleNum(mTupleNum);

    std::ifstream dataFile;
    dataFile.open(mPath, std::ios::in);
    
    for (auto& attr : mAttrs)
    {
        std::vector<int> attrData(mTupleNum);
        for (size_t tupleIndex = 0; tupleIndex < mTupleNum; tupleIndex++)
        {
            int data;
            dataFile >> data;
            attrData[tupleIndex] = data;
        }

        relation.Insert(attr, std::move(attrData));
    }

    std::cout << mPath << " loaded." << std::endl;

    return relation;
}


