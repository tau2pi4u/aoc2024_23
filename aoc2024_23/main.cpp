#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <list>
#include <memory>
#include <set>
#include "utils.hpp"

#define TESTING 0
#if TESTING
#define INFILE "testInput.txt"
#else
#define INFILE "input.txt"
#endif

int GetNextId()
{
	static int id = 0;
	return id++;
}

struct Node
{
	Node(std::string const& name) : name(name), id(GetNextId()) {}

	std::string name;
	int id;

	std::unordered_set<Node*> edges;
};

using clique_t = std::vector<Node*>;
//using clique_t = std::list<Node*>;

struct NodeSorter
{
	bool operator()(Node const* lhs, Node const* rhs) const
	{
		if (lhs && rhs)
		{
			return lhs->id < rhs->id;
		}
		return lhs > rhs;
	}
};

struct Triplet
{
	Triplet(std::vector<Node*>  in)
	{
		std::sort(in.begin(), in.end(), [](Node const* lhs, Node const* rhs) {return lhs->id < rhs->id; });
		a = in[0];
		b = in[1];
		c = in[2];
	}

	Node* a;
	Node* b;
	Node* c;
};

bool operator==(Triplet const& lhs, Triplet const& rhs)
{
	return lhs.a == rhs.a && lhs.b == rhs.b && lhs.c == rhs.c;
}

//https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
template <class T>
inline void hash_combine(std::size_t& seed, const T& v)
{
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

struct TripletHasher
{
	size_t operator()(Triplet const& triplet) const
	{
		size_t hash = 0;
		hash_combine(hash, triplet.a->id);
		hash_combine(hash, triplet.b->id);
		hash_combine(hash, triplet.c->id);
		return hash;
	}
};

struct VectorHasher
{
	size_t operator()(std::vector<Node*> nodes) const
	{
		size_t hash = 0;
		std::sort(nodes.begin(), nodes.end(), NodeSorter());

		for (auto const& node : nodes)
		{
			hash_combine(hash, node->id);
		}

		return hash;
	}

	size_t operator()(std::list<Node*> nodes) const
	{
		size_t hash = 0;
		nodes.sort([](Node const* lhs, Node const* rhs) { return lhs->id < rhs->id; });

		for (auto const& node : nodes)
		{
			hash_combine(hash, node->id);
		}

		return hash;
	}

	size_t operator()(std::set<Node*, NodeSorter> const& nodes) const
	{
		size_t hash = 0;

		for (auto const& node : nodes)
		{
			hash_combine(hash, node->id);
		}

		return hash;
	}
};

struct Graph
{
	std::vector<std::unique_ptr<Node>> nodes;
	std::unordered_map<std::string, Node*> nodesByName;
};

Graph BuildGraph(std::vector<std::string> const& inputLines)
{
	Graph g;
	auto get_node = [&](std::string const& name)
	{
		if (g.nodesByName.count(name) == 0)
		{
			g.nodes.push_back(std::make_unique<Node>(name));
			g.nodesByName[name] = g.nodes.back().get();
		}
		return g.nodesByName.at(name);
	};

	for (auto const& line : inputLines)
	{
		auto nodeAName = line.substr(0, 2);
		auto nodeBName = line.substr(3);

		auto nodeA = get_node(nodeAName);
		auto nodeB = get_node(nodeBName);

		nodeA->edges.insert(nodeB);
		nodeB->edges.insert(nodeA);
	}

	return g;
}

bool IsConnected(Node* a, Node* b)
{
	return a->edges.count(b);
}

size_t CountTriosWithT(Graph const& g)
{
	std::unordered_set<Triplet, TripletHasher> triplets;

	for (auto const& nodeA : g.nodes)
	{
		if (nodeA->name[0] != 't') continue;
		for (auto const& nodeB : nodeA->edges)
		{
			for (auto const& nodeC : nodeB->edges)
			{
				if (nodeC == nodeA.get()) continue;
				if (nodeC->edges.end() == std::find(nodeC->edges.begin(), nodeC->edges.end(), nodeA.get()))
				{
					continue;
				}

				auto triplet = Triplet({ nodeA.get(), nodeB, nodeC});
				if (triplets.count(triplet)) continue;
				triplets.insert(triplet);

				if (!IsConnected(nodeA.get(), nodeB)) Unreachable();
				if (!IsConnected(nodeB, nodeC)) Unreachable();
				if (!IsConnected(nodeA.get(), nodeC)) Unreachable();

				//printf("%s, %s, %s\n", nodeA->name.c_str(), nodeB->name.c_str(), nodeC->name.c_str());
			}
		}
	}
	return triplets.size();
}

template <typename T>
void CliqueInsert(T& clique, Node * node)
	requires(std::is_same<T, std::vector<Node*>>::value)
{
	clique.push_back(node);
	//std::sort(clique.begin(), clique.end(), NodeSorter());
}

template <typename T>
void CliqueInsert(T& clique, Node* node)
	requires(std::is_same<T, std::list<Node*>>::value)
{
	auto itr = std::find(clique.rbegin(), clique.rend(), nullptr);
	if(itr == clique.rend())
	{
		clique.push_back(node);
		clique.sort(NodeSorter());
		return;
	}
	*itr = node;
}

template <typename T>
void CliqueErase(T& clique, Node* node)
	requires(std::is_same<T, std::vector<Node*>>::value || std::is_same<T, std::list<Node*>>::value)
{
	auto itr = std::find(clique.begin(), clique.end(), node);
	if (itr != clique.end()) *itr = nullptr;
}

void BuildFullyConnectedSet(Graph const& g, Node* lastAdded, clique_t & set, clique_t & best)
{
	static std::unordered_set<size_t> seen;

	size_t hash = VectorHasher()(set);
	if (seen.count(hash)) return;
	seen.insert(hash);

	if (set.size() > best.size())
	{
		best = set;
	}

	for (auto& node : lastAdded->edges)
	{
		if (set.end() != std::find(set.begin(), set.end(), node)) continue;

		bool stillFullyConnected = true;
		for (auto& oNode : set)
		{
			stillFullyConnected &= IsConnected(node, oNode);
		}
		if (!stillFullyConnected) continue;

		size_t setSize = set.size();

		CliqueInsert(set, node);

		BuildFullyConnectedSet(g, node, set, best);

		CliqueErase(set, node);
	}
}

std::vector<Node*> BuildFullyConnectedSet(Graph const& g)
{
	clique_t best;
	std::vector<Node*> bestVec;
	for (auto const& node : g.nodes)
	{
		clique_t current = { node.get() };
		BuildFullyConnectedSet(g, node.get(), current, best);
	}

	bestVec.insert(bestVec.begin(), best.begin(), best.end());

	std::sort(bestVec.begin(), bestVec.end(), [](Node* lhs, Node* rhs) {return lhs->name < rhs->name; });

	return bestVec;
}

#include <chrono>

#define TIME std::chrono::high_resolution_clock::now();
#define DIFF(a, b) (std::chrono::duration_cast<std::chrono::microseconds>(b-a).count())

int main()
{
	auto t0 = TIME;
	auto inputLines = GetInputAsString(INFILE);
	auto t1 = TIME;
	auto g = BuildGraph(inputLines);
	auto t2 = TIME;
	auto p1 = CountTriosWithT(g);
	auto t3 = TIME;
	auto best = BuildFullyConnectedSet(g);
	auto t4 = TIME;


	printf("p1: %zu\n", p1);
	for (auto const& node : best)
	{
		printf("%s,", node->name.c_str());
	}
	printf("\n");

	printf("io: %lld\n", DIFF(t0, t1));
	printf("gb: %lld\n", DIFF(t1, t2));
	printf("p1: %lld\n", DIFF(t2, t3));
	printf("p2: %lld\n", DIFF(t3, t4));
}