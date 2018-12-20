#pragma once

#include "Module.h"
#include "net/Net.h"
#include "Node.h"
#include "MCC.h"
#include "MCP.h"
#include <map>

class ModuleNodeCluster : public Module, public TCPNetworkManagerDelegate
{
public:

	// Virtual methods from parent class Module

	bool init() override;

	bool start() override;

	bool update() override;

	bool updateGUI() override;

	bool cleanUp() override;

	bool stop() override;


	// TCPNetworkManagerDelegate virtual methods

	void OnAccepted(TCPSocketPtr socket) override;

	void OnPacketReceived(TCPSocketPtr socket, InputMemoryStream &stream) override;

	void OnDisconnected(TCPSocketPtr socket) override;


	// User criteria

	int MaxDepth() const { return max_depth; }

	int MaxNearest() const { return max_mcc_iterations; }

	// Negociations

	bool NodeMissingConstraint(uint16_t agentID, uint16_t constraintItemId);
	void AddConstraintToNode(uint16_t agentID, uint16_t constraintItemId);
	void WithdrawFromNode(uint16_t agentID, uint16_t itemId);

private:

	bool startSystem();

	void runSystem();

	void stopSystem();

	void spawnMCP(int nodeId, int requestedItemId, int contributedItemId);

	void spawnMCC(int nodeId, int contributedItemId, int constraintItemId);

	std::vector<NodePtr> _nodes; /**< Array of nodes spawn in this host. */

	int state = 0; /**< State machine. */


	// User criteria

	int max_depth = 10; // mcp max depth

	int max_mcc_iterations = 5; // mcp checks x nearest mcc's

	// Negociations

	std::map<uint16_t, std::vector<uint16_t>> negotiations;

};
