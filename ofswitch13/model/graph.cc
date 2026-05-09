/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "graph.h"


NS_LOG_COMPONENT_DEFINE ("OFSwitch13RoutingGraph");

namespace ns3 {
using namespace boost;


Routing_app::Routing_app(/* args */)
{
}

Routing_app::~Routing_app()
{
}

int vertex_count = 0;

// struct Edge{std::string blah;};
// typedef boost::adjacency_list<boost::listS, boost::vecS, boost::undirectedS, boost::no_property, Edge > Graph;


// typedef boost::property<boost::edge_weight_t, int> EdgeWeightProperty;
// typedef boost::adjacency_list<boost::listS, boost::vecS,boost::undirectedS,boost::no_property,EdgeWeightProperty> UndirectedGraph;
// typedef boost::graph_traits<UndirectedGraph>::edge_iterator edge_iterator;

//UndirectedGraph g;
// Graph g;


/*std::ostringstream o;
o << dpid;
o << port_no;
str += o.str();
*/
// nodes_map[dpid]

int Routing_app::AddNode(uint64_t dpid, uint32_t port){
    std::ostringstream o;
    o << dpid << port;
    return AddNode(o.str());
}

int Routing_app::AddNode(std::string uuid){
    // int n = num_vertices(g);
    // int index = n;
    // // check if exists and insert in map
    // std::pair<std::map<std::string, struct node_uid>::iterator,bool> ret;
    // node_uid a_{n, uuid, NULL, NULL};
    // ret = uuid2index.insert ( std::pair<std::string, struct node_uid>(uuid,a_) );
    // if(ret.second==false){
    //     std::cout << "element 'z' already existed";
    //     std::cout << " with a value of " << ret.first->second.vertex_id << '\n';
    //     index = ret.first->second.vertex_id;
    //     return index;
    // }

    // //insert in graph
    // VertexDescriptor a = add_vertex(VertexPropertyType(uuid), g);
    // //assert(a == index);
    // if (a.vertex_id != index) return -1;
    // return index;
    return 0;
}

// TODO check each x time for links if the time has passed, and is no longer alive

int Routing_app::AddNode(){
    // mtx.lock();
    int n_ = add_vertex(g);

    std::cout <<"BOOOO2 "<< g.m_vertices.size() << " " << boost::num_vertices(g) << " " << &g << std::endl;
    //VertexDescriptor n_ = add_vertex(g);
    //mtx.unlock();
    std::cout << g.m_vertices.size() << " " << boost::num_vertices(g) << " INSERT NODE " <<  n_ << std::endl;


    return static_cast<int>(n_);
}
bool Routing_app::EdgeExists(int a, int c){
    return edge(a,c,g).second;
}
int Routing_app::AddNode(Ipv4Address ip){

    //int n_ = add_vertex(g);
    int n_ = AddNode();
    struct node_uid p;
    p.vertex_id = n_;
    p.ipv4_addr = ip;
    p.isValid = true;
    m[n_] = p;
    //std::cout << m[n_].ipv4_addr << std::endl;
    std::cout << "Node-  " << n_ << " IP " <<  ip << std::endl;
    return n_;
}
int Routing_app::AddNode(Mac48Address mac){
    //int n_ = add_vertex(g);
    int n_ = AddNode();
    std::cout << "Node-  " << n_ << " MAC " <<  mac << std::endl;
    struct node_uid p;
    p.vertex_id = n_;
    p.hw_addr = mac;
    p.isValid = true;
    m[n_] = p;
    //std::cout << g.m_vertices.size() << " " << count << " INSERT NODE " <<  n_ << std::endl;

    return n_;
}

// void Routing_app::AddLink(VertexDescriptor a, VertexDescriptor c, int weight){
//     add_edge(a, c, EdgePropertyType(weight), g);
// }
void Routing_app::AddLink(int a, int c, int weight){
    std::pair<Edge, bool> ed = boost::edge(a,c,g);
    if(!ed.second )
        add_edge(a, c, EdgePropertyType(weight), g);
    else{
        //int weight = get(boost::edge_weight_t(), g, ed.first);
        //int weightToAdd = 10;
        //boost::put(boost::edge_weight_t(), g, ed.first, weight+weightToAdd);
        boost::put(boost::edge_weight_t(), g, ed.first, weight);
    }
}

VertexDescriptor Routing_app::GetNodeByMac48(Mac48Address node){
    //find index
    int index;
    auto it = mac2index.find(node);
    if (it == mac2index.end() ) {
        return 0;
    } else {
        //index = mac2index[node]; // it creates the key if it does not exists ?
        index = mac2index.at(it->first);
    }


    //Returns the nth vertex in the graph's vertex list.
    return vertex(index, g);
}

int Routing_app::GetNode(Ipv4Address ip){
    // mtx.lock();
    for (auto it = m.begin(); it != m.end(); it++){

        if ( (*it).second.ipv4_addr == ip ){
            //std::cout << "key index " << (*it).first << std::endl;
            //mtx.unlock();
            return (*it).first ;
        }
    }
    //mtx.unlock();
    return -1;
}

int Routing_app::GetNode(Mac48Address mac){
    // mtx.lock();
    for (auto it = m.begin(); it != m.end(); it++){
        if ( (*it).second.hw_addr == mac ){
            //std::cout << "key index " << (*it).first << std::endl;
            //mtx.unlock();
            return (*it).first ;
        }
    }
    //mtx.unlock();
    return -1;
}

int Routing_app::GetNode(uint64_t dpid, uint32_t port ){
    for (auto it = m.begin(); it != m.end(); it++){
        //std::cout << "key" << (*it).first <<" ";
        if ( (*it).second.datapath == dpid && (*it).second.port_no == port){
            //std::cout << (*it).first <<std::endl;
            return (*it).first ;
        }
    }

    return -1;
}


// DirectedGraphType Routing_app::sample_makeDirectedGraphWithCycles()
// {
//     DirectedGraphType g;

//     VertexDescriptor a = add_vertex(VertexPropertyType("a"), g);
//     VertexDescriptor b = add_vertex(VertexPropertyType("b"), g);
//     VertexDescriptor c = add_vertex(VertexPropertyType("c"), g);
//     VertexDescriptor d = add_vertex(VertexPropertyType("d"), g);
//     VertexDescriptor e = add_vertex(VertexPropertyType("e"), g);

//     add_edge(a, c, EdgePropertyType(1), g);
//     add_edge(b, d, EdgePropertyType(1), g);
//     add_edge(b, e, EdgePropertyType(2), g);
//     add_edge(c, b, EdgePropertyType(5), g);
//     add_edge(c, d, EdgePropertyType(10), g);
//     add_edge(d, e, EdgePropertyType(4), g);
//     add_edge(e, a, EdgePropertyType(3), g);
//     add_edge(e, b, EdgePropertyType(7), g);

//     return g;
// }

std::vector<int> Routing_app::djikstra(int source,  int destination)
{


    //add source & target
    VertexDescriptor startV = boost::vertex(source, g );
    VertexDescriptor endV = boost::vertex(destination, g );

//     //predecessors
//     // Output for predecessors of each node in the shortest path tree result
//     std::vector predMap(boost::num_vertices(g));

//     //distMap
//     // Output for distances for each node with initial size
//     // of number of vertices
//     std::vector distMap(boost::num_vertices(g));

// //solve shortest path problem
// boost::dijkstra_shortest_paths(g, startV,
//   weight_map(boost::get(&Arc::cost, g)) //arc costs from bundled properties
//   .predecessor_map(boost::make_iterator_property_map(predMap.begin(),//property map style
//                                                     boost::get(boost::vertex_index, g)))
//   .distance_map(boost::make_iterator_property_map(distMap.begin(),//property map style
//                                                     boost::get(boost::vertex_index, g)))
//   );


    const int numVertices = num_vertices(g);
    std::vector<int> distances(numVertices);
    std::vector<VertexDescriptor> pMap(numVertices);



    // auto distanceMap = predecessor_map(
    //     make_iterator_property_map(pMap.begin(), get(vertex_index, g))).distance_map(
    //     make_iterator_property_map(distances.begin(), get(vertex_index, g)));
    auto distanceMap = predecessor_map(&pMap[0]).distance_map(&distances[0]);
    dijkstra_shortest_paths(g, source, distanceMap);

    //----- print
    std::vector<int> path;
    VertexDescriptor current = endV;
    while (startV != current)
    {
        path.push_back(current);
        current = pMap[current];
    }
    // add start as last element (=start node) to path
    path.push_back(startV);
    //print out the path with reverse iterator
    //std::vector<VertexDescriptor>::reverse_iterator rit;
    std::cout <<"Path from "<< startV << " to "<< endV << " is: "<< std::endl;
    double totalCost = distances[endV];
    for (auto rit = path.rbegin(); rit != path.rend(); ++rit)
        std::cout << *rit << " -> ";

    std::cout << std::endl;
    std::cout << "Total Cost: "<< totalCost << std::endl;
    //----

    //return getPath(g, pMap, source, destination);
    //return pMap.rbegin()[1];
    return path;
}

void Routing_app::PrintGraph(){
    std::cout << "Number of edges = " << num_edges(g) << "\n";
    std::cout << "Edge list:\n";

    boost::property_map<UndirectedGraphType, boost::edge_weight_t>::type EdgeWeightMap = get(boost::edge_weight_t(), g);

    std::pair<edge_iterator, edge_iterator> ei = edges(g);

    for (edge_iterator it = ei.first; it != ei.second; ++it )
    {
        //std::cout << it->m_source  << " " << it->m_target << " " << it->boost::edge_weight_t() << std::endl;
        std::cout << *it << " " << EdgeWeightMap[*it] << std::endl;
    }

    std::cout << std::endl;
}

/*
void Routing_app::PrintPath(MyAlgorithm::Path const& path, graph_t const& g) {
    std::cout << "Path: ";
    auto idmap = get(boost::vertex_name, g);
    auto wmap = get(boost::edge_weight, g);

    auto previous = g.null_vertex();
    for (auto v : path) {
        if (previous != g.null_vertex()) {
            for (auto e : make_iterator_range(out_edges(previous, g))) {
                if (target(e, g) == v) {
                    std::cout << " -> (w:" << " << " << wmap[e] << ") ";
                }
            }
        }
        std::cout << "#" << v << " (id:" << idmap[v] << ") ";
        previous = v;
    }
    std::cout << "\n";
}
*/
}; // namespace ns3
