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

uint16_t GetNextId()
{
	static uint16_t id = 0;
	return ++id;
}

uint16_t GetNextTag()
{
	static uint16_t tag = 0;
	return ++tag;
}


struct Node
{
	Node(std::string const& name) : nameAsInt(GetNameAsInt(name)), id(GetNextId()), tag(0) {}
	Node(uint16_t nameAsInt) : nameAsInt(nameAsInt), id(GetNextId()), tag(0) {}

	static int GetNameAsInt(std::string const& name)
	{
		return (name[0] << 8) | name[1];
	}

	uint16_t nameAsInt;
	uint16_t id;
	uint16_t tag;

	std::vector<Node*> edges;
};

using clique_t = std::vector<Node*>;

struct NodeSorter
{
	bool operator()(Node const* lhs, Node const* rhs) const
	{
		if (lhs && rhs)
		{
			return lhs->nameAsInt < rhs->nameAsInt;
		}
		return lhs > rhs;
	}
};

struct NodeIdSorter
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
		std::sort(in.begin(), in.end(), NodeSorter());
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
		hash_combine(hash, triplet.a->nameAsInt);
		hash_combine(hash, triplet.b->nameAsInt);
		hash_combine(hash, triplet.c->nameAsInt);
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
			hash_combine(hash, node->nameAsInt);
		}

		return hash;
	}
};

struct Graph
{
	std::vector<std::unique_ptr<Node>> nodes;
	std::unordered_map<uint16_t, Node*> nodesByName;
};

Graph BuildGraph(std::vector<std::string> const& inputLines)
{
	Graph g;
	auto get_node = [&](std::string const& name)
	{
		auto nameAsInt = Node::GetNameAsInt(name);
		if (g.nodesByName.count(nameAsInt) == 0)
		{
			g.nodes.push_back(std::make_unique<Node>(nameAsInt));
			g.nodesByName.insert({ nameAsInt, g.nodes.back().get() });
		}
		return g.nodesByName.at(nameAsInt);
	};

	for (auto const& line : inputLines)
	{
		auto nodeAName = line.substr(0, 2);
		auto nodeBName = line.substr(3);

		auto nodeA = get_node(nodeAName);
		auto nodeB = get_node(nodeBName);

		nodeA->edges.push_back(nodeB);
		nodeB->edges.push_back(nodeA);
	}

	return g;
}

void SortEdges(Graph& g)
{
	for (auto& node : g.nodes)
	{
		std::sort(node->edges.begin(), node->edges.end(), NodeIdSorter());
	}
}

bool IsConnected(Node* a, Node* b)
{
	auto itr = std::find(a->edges.begin(), a->edges.end(), b);
	return itr != a->edges.end();
}

std::string NameIntToStr(uint16_t nameAsInt)
{
	std::string out;
	out += static_cast<char>(nameAsInt >> 8);
	out += static_cast<char>(nameAsInt & 0xff);
	return out;
}

size_t CountTriosWithT(Graph const& g)
{
	std::unordered_set<Triplet, TripletHasher> triplets;

	for (auto const& nodeA : g.nodes)
	{
		for (auto const& nodeB : nodeA->edges)
		{
			if (nodeB->id >= nodeA->id) continue;

			for (auto const& nodeC : nodeB->edges)
			{
				if (nodeC->id >= nodeB->id) continue;

				if (!IsConnected(nodeA.get(), nodeC))
				{
					continue;
				}

				if (!(NameIntToStr(nodeA->nameAsInt)[0] == 't' || NameIntToStr(nodeB->nameAsInt)[0] == 't' || NameIntToStr(nodeC->nameAsInt)[0] == 't'))
				{
					continue;
				}

				auto triplet = Triplet({ nodeA.get(), nodeB, nodeC });
				if (triplets.count(triplet)) continue;
				triplets.insert(triplet);
			}
		}
	}
	return triplets.size();
}

Node* LastNodeNotNull(std::vector<Node*> & clique)
{
	return *std::find_if(clique.rbegin(), clique.rend(), [](Node * node) { return !!node; });
}

void BuildFullyConnectedSet(Graph const& g, Node* lastAdded, clique_t & set, clique_t & best)
{
	for (auto& node : set.front()->edges)
	{
		if (node->id <= set.back()->id) continue;

		bool stillFullyConnected = true;
		for (auto& oNode : set)
		{
			stillFullyConnected &= IsConnected(node, oNode);
			if (!stillFullyConnected) break;
		}
		if (!stillFullyConnected) continue;

		set.push_back(node);
	}

	if (set.size() > best.size())
	{
		best = set;
	}
}

std::vector<Node*> BuildFullyConnectedSet(Graph const& g)
{
	clique_t best;
	std::vector<Node*> bestVec;

	clique_t current;

	for (auto const& node : g.nodes)
	{
		current = { node.get() };
		BuildFullyConnectedSet(g, node.get(), current, best);
	}

	bestVec.insert(bestVec.begin(), best.begin(), best.end());

	std::sort(bestVec.begin(), bestVec.end(), [](Node* lhs, Node* rhs) {return lhs->nameAsInt < rhs->nameAsInt; });

	return bestVec;
}

#include <chrono>

#define TIME std::chrono::high_resolution_clock::now();
#define DIFF(a, b) (std::chrono::duration_cast<std::chrono::microseconds>(b-a).count())

int main()
{
	auto t0 = TIME;
	auto inputLines = GetInputAsString(INFILE);
	auto ts = TIME;
	auto g = BuildGraph(inputLines);
	auto t1 = TIME;
	SortEdges(g);
	auto t2 = TIME;
	auto p1 = CountTriosWithT(g);
	auto t3 = TIME;
	auto best = BuildFullyConnectedSet(g);
	auto t4 = TIME;


	printf("p1: %zu\n", p1);
	for (auto const& node : best)
	{
		printf("%s,", NameIntToStr(node->nameAsInt).c_str());
	}
	printf("\n");

	printf("io: %lld\n", DIFF(t0, ts));
	printf("gb: %lld\n", DIFF(ts, t1));
	printf("es: %lld\n", DIFF(t1, t2));
	printf("p1: %lld\n", DIFF(t2, t3));
	printf("p2: %lld\n", DIFF(t3, t4));
}