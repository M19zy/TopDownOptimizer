#pragma once

#include "Relation.h"
#include <string>


class Schema
{
public:
    Schema(std::string_view path)
        : mPath(path)
    {}

    std::pair<std::vector<std::string>, std::vector<std::string>> Load();

private:
    std::string mPath;
};


class RelationDesc
{
public:
    RelationDesc(std::string_view path)
        : mPath(path)
    {}

    Relation Load();

private:
    std::string mPath;
};


class RawRelation
{
public:
    RawRelation(std::string_view path, const std::vector<std::string>& attrs, size_t tupleNum)
        : mPath(path), mAttrs(attrs), mTupleNum(tupleNum)
    {}

    Relation Load();

private:
    std::string mPath;
    std::vector<std::string> mAttrs;
    size_t mTupleNum;
};


