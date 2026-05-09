/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef ROUTING_GRAPH_H
#define ROUTING_GRAPH_H

#include <boost/graph/adjacency_list.hpp>
#include <ns3/application.h>
#include <ns3/socket.h>
#include <string>
#include <map>

//#include <boost/graph/undirected_dfs.hpp>
// #include<boost/graph/properties.hpp>
// #include <boost/graph/named_function_params.hpp>            //for named parameter http://www.boost.org/doc/libs/1_58_0/libs/graph/doc/bgl_named_params.html
// #include <boost/cstdlib.hpp>

#include <boost/graph/graph_traits.hpp>
// #include <boost/graph/undirected_graph.hpp>                                                 // for exit_success;

#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/dag_shortest_paths.hpp>
#include <boost/graph/graph_utility.hpp>
#include <vector>

#include <mutex>          // std::mutex

namespace ns3 {
using namespace boost;

typedef property<vertex_name_t, std::string> VertexPropertyType;
typedef property<edge_weight_t, int> EdgePropertyType;
//typedef adjacency_list<vecS, vecS, directedS, VertexPropertyType, EdgePropertyType> DirectedGraphType;
typedef adjacency_list<vecS, vecS, directedS, boost::no_property, EdgePropertyType> DirectedGraphType;
typedef adjacency_list<vecS, vecS, undirectedS, boost::no_property, EdgePropertyType> UndirectedGraphType;

typedef graph_traits<UndirectedGraphType>::vertex_descriptor VertexDescriptor;
typedef boost::graph_traits<UndirectedGraphType>::edge_iterator edge_iterator;

typedef UndirectedGraphType::edge_descriptor Edge;



struct node_uid
{
    bool isValid = false;
    bool isOF = false;
    int vertex_id;
    uint64_t datapath;
    uint32_t port_no ;
    std::string uuid;
    Mac48Address hw_addr;
    Ipv4Address ipv4_addr;
};


class Routing_app
{
private:
    /* data */
public:
    Routing_app(/* args */);
    ~Routing_app();

    /**
     * @brief add vertex to graph
     *
     * @param uuid datapathid concatenated with port as universal unique identifier
     * @return int -1 if error, >=0 as index on graph
     */
    int AddNode(std::string uuid);
    int AddNode(uint64_t dpid, uint32_t port);
    int AddNode();
    int AddNode(Ipv4Address ip);
    int AddNode(Mac48Address mac);

    bool EdgeExists(int a, int c);

    void PrintGraph();
    /**
     * @brief add link between a and c with weight of x
     *
     * @param a
     * @param c
     * @param weight
     */
    //void AddLink(VertexDescriptor a, VertexDescriptor c, int weight);
    void AddLink(int a, int c, int weight);


    /**
     * @brief Dijkstra’s algorithm to find the shortest path between source and destination
     *
     * @param source
     * @param destination
     * @return std::vector<VertexDescriptor>
     */
    std::vector<int> djikstra(int source,  int destination);

    //void PrintPath(MyAlgorithm::Path const& path, graph_t const& g);

    /**
     * @brief Get the Node By Mac48Address
     *
     * @param node
     * @return VertexDescriptor
     */
    VertexDescriptor GetNodeByMac48(Mac48Address node);


    /**
     * @brief Get the Node index on graph
     *
     * @param ip /mac
     * @return int
     */
    int GetNode(Ipv4Address ip);
    int GetNode(Mac48Address mac);
    int GetNode(uint64_t dpid, uint32_t port );


    //DirectedGraphType sample_makeDirectedGraphWithCycles();

    // ----- variables ----

    /*never delete a node!! or else the vector list will be invalid, as the indexes will point to other structures*/
    UndirectedGraphType g;
    //boost::adjacency_list<boost::listS, boost::vecS,boost::undirectedS, boost::no_property, EdgePropertyType > g;

    /**
     * @brief map dpid+port_no to node Object
     *
     */
    std::map<std::string, struct node_uid> uuid2index;

    /**
     * @brief map mac48Address to vertex_index
     *
     */
    std::map<Mac48Address, int> mac2index;
    /**
     * @brief map vertex_index to node Object
     *
     */
    std::map < int, struct node_uid> m;
    int count = 0;

};
}
#endif /* ROUTING_GRAPH_H */