//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. Written by Alfredo
// Gimenez (alfredo.gimenez@gmail.com). LLNL-CODE-663358. All rights
// reserved.
//
// This file is part of MemAxes. For details, see
// https://github.com/scalability-tools/MemAxes
//
// Please also read this link – Our Notice and GNU Lesser General Public
// License. This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License (as
// published by the Free Software Foundation) version 2.1 dated February
// 1999.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and
// conditions of the GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//
// OUR NOTICE AND TERMS AND CONDITIONS OF THE GNU GENERAL PUBLIC LICENSE
// Our Preamble Notice
// A. This notice is required to be provided under our contract with the
// U.S. Department of Energy (DOE). This work was produced at the Lawrence
// Livermore National Laboratory under Contract No. DE-AC52-07NA27344 with
// the DOE.
// B. Neither the United States Government nor Lawrence Livermore National
// Security, LLC nor any of their employees, makes any warranty, express or
// implied, or assumes any liability or responsibility for the accuracy,
// completeness, or usefulness of any information, apparatus, product, or
// process disclosed, or represents that its use would not infringe
// privately-owned rights.
//////////////////////////////////////////////////////////////////////////////

#include "datacluster.h"

#include <vector>

DataClusterTree::DataClusterTree()
{
}

DataClusterTree::~DataClusterTree()
{
}

void DataClusterTree::build(DataObject *d, int dim, METRIC_TYPE m)
{
    std::vector<DataClusterLeafNode*> leafNodes;
    int delta = 600;
    int w = 1000;
    leafNodes = createWindows(d,dim,delta,w);
    hierarchicalCluster(d,leafNodes,m);
}

std::vector<DataClusterLeafNode*> DataClusterTree::createWindows(DataObject *d, int dim, int delta, int winSize)
{
    std::vector<DataClusterLeafNode*> leafNodes;

    // IndexList *sortedSamples = d->getSortedList(dim);

    // // Create leaf node for every window
    // for(unsigned int s=0; s<sortedSamples->size(); s+=delta)
    // {
    //     DataClusterLeafNode *newLeaf = new DataClusterLeafNode();
    //     qreal winMin = sortedSamples->at(s).val;
    //     qreal winMax = winMin;
    //     for(unsigned int sidx=s; sidx<s+winSize; sidx++)
    //     {
    //         if(sidx >= sortedSamples->size())
    //             break;

    //         newLeaf->samples.insert(sortedSamples->at(sidx).idx);
    //         winMax = sortedSamples->at(sidx).val;
    //     }

    //     if(newLeaf->samples.empty())
    //         break;

    //     newLeaf->setRange(winMin,winMax);
    //     leafNodes.push_back(newLeaf);
    // }

    // // merge last two leaves (better to have one bigger than one smaller
    // leafNodes.at(leafNodes.size()-2)->samples.insert(
    //             leafNodes.back()->samples.begin(),
    //             leafNodes.back()->samples.end());
    // leafNodes.erase(leafNodes.end()-1);

    return leafNodes;
}

std::vector<DataClusterInternalNode *> DataClusterTree::createInternalFromLeaves(DataObject *d, std::vector<DataClusterLeafNode*> &leafNodes)
{
    std::vector<DataClusterInternalNode*> levelNodes;
    // for(unsigned int i=0; i<leafNodes.size(); i++)
    // {
    //     DataClusterInternalNode *newNode = new DataClusterInternalNode();
    //     newNode->aggregate = new HardwareClusterAggregate();
    //     //TODO newNode->aggregate->createAggregateFromSamples(d,&leafNodes.at(i)->samples);
    //     newNode->setRange(leafNodes.at(i)->rangeMin,leafNodes.at(i)->rangeMax);

    //     leafNodes.at(i)->parent = newNode;
    //     newNode->children.push_back(leafNodes.at(i));

    //     levelNodes.push_back(newNode);
    // }
    return levelNodes;
}

std::vector<DataClusterNode *> DataClusterTree::getNodesAtDepth(int depth)
{
    return root->getNodesAtDepth(depth);
}

void DataClusterTree::hierarchicalCluster(DataObject *d, std::vector<DataClusterLeafNode *> &leafNodes, METRIC_TYPE m)
{
    // Create an internal node for each leaf
    std::vector<DataClusterInternalNode*> levelNodes;
    levelNodes = createInternalFromLeaves(d,leafNodes);

    // Store distances between nodes
    std::vector<qreal> nodeDistances;
    nodeDistances.resize(levelNodes.size()-1);
    for(unsigned int i=0; i<levelNodes.size()-1; i++)
    {
        HardwareClusterAggregate *agg1 = (HardwareClusterAggregate*)levelNodes.at(i)->aggregate;
        HardwareClusterAggregate *agg2 = (HardwareClusterAggregate*)levelNodes.at(i+1)->aggregate;
        qreal dist = agg1->distance(agg2,m);
        nodeDistances[i] = dist;
    }

    // Merge up from internal nodes created
    while(levelNodes.size() > 1)
    {
        // Get smallest distance
        std::vector<qreal>::iterator minDistIt;
        minDistIt = std::min_element(nodeDistances.begin(),nodeDistances.end());

        // Get node with smallest distance
        int n1NodeIndex = std::distance(nodeDistances.begin(),minDistIt);
        int n2NodeIndex = n1NodeIndex + 1;

        // merge minDistIndex , minDistIndex+1
        DataClusterInternalNode *n1 = levelNodes.at(n1NodeIndex);
        DataClusterInternalNode *n2 = levelNodes.at(n2NodeIndex);

        // into new node
        DataClusterInternalNode *newNode = new DataClusterInternalNode();
        n1->parent = newNode;
        n2->parent = newNode;
        newNode->children.push_back(n1);
        newNode->children.push_back(n2);
        newNode->setRange(std::min(n1->rangeMin,n2->rangeMin),
                          std::max(n1->rangeMax,n2->rangeMax));

        // with combined aggregate
        HardwareClusterAggregate *newAgg = new HardwareClusterAggregate();
        newAgg->initFrom(d,(HardwareClusterAggregate*)n1->aggregate);
        newAgg->combineAggregate(d,(HardwareClusterAggregate*)n1->aggregate);
        newAgg->combineAggregate(d,(HardwareClusterAggregate*)n2->aggregate);
        newNode->aggregate = newAgg;

        // recalculate new distances
        if(n1NodeIndex > 0)
        {
            // replace distance (before n1) to (n1)
            // with (before n1) to (newNode)
            DataClusterInternalNode *beforen1 = levelNodes.at(n1NodeIndex-1);
            HardwareClusterAggregate *beforen1agg = (HardwareClusterAggregate*) beforen1->aggregate;
            qreal newDist = beforen1agg->distance(newAgg,m);

            nodeDistances[n1NodeIndex] = newDist;
        }
        if(n2NodeIndex < (int)levelNodes.size()-1)
        {
            // replace distance (n2) to (after n2)
            // with (newNode) to (after n2)
            DataClusterInternalNode *aftern1 = levelNodes.at(n2NodeIndex+1);
            HardwareClusterAggregate *aftern1agg = (HardwareClusterAggregate*) aftern1->aggregate;
            qreal newDist = newAgg->distance(aftern1agg,m);

            nodeDistances[n2NodeIndex] = newDist;
        }

        // remove n1 and n2 from levelNodes
        nodeDistances.erase(minDistIt);

        // replace n1, n2 with newNode
        levelNodes.erase(levelNodes.begin()+n1NodeIndex,levelNodes.begin()+n1NodeIndex+2);
        levelNodes.insert(levelNodes.begin()+n1NodeIndex,newNode);
    }

    DataClusterNode *lastNode = levelNodes.front();
    root = lastNode;
}

DataClusterNode::DataClusterNode()
{
}

DataClusterNode::~DataClusterNode()
{
}

std::vector<DataClusterNode *> DataClusterNode::getNodesAtDepth(int depth)
{
    std::vector<DataClusterNode*> nodes;
    if(isLeaf())
    {
        // add last parent before this
        nodes.push_back(this->parent);
        return nodes;
    }

    if(depth == 0)
    {
        nodes.push_back(this);
        return nodes;
    }

    // root is internal, we can cast
    DataClusterInternalNode *inode = (DataClusterInternalNode*)this;
    for(unsigned int i=0; i<inode->children.size(); i++)
    {
        std::vector<DataClusterNode*> chnodes;
        chnodes = inode->children.at(i)->getNodesAtDepth(depth-1);

        nodes.insert(nodes.end(),chnodes.begin(),chnodes.end());
    }

    return nodes;
}

DataClusterInternalNode::DataClusterInternalNode()
{
}

DataClusterInternalNode::~DataClusterInternalNode()
{
}

DataClusterLeafNode::DataClusterLeafNode()
{
}

DataClusterLeafNode::~DataClusterLeafNode()
{
}
