#pragma once
#include "Agent.h"

// Forward declaration
class MCP;
using MCPPtr = std::shared_ptr<MCP>;

class UCP :
	public Agent
{
public:

	// Constructor and destructor
	UCP(Node *node, uint16_t requestedItemId, uint16_t contributedItemId, const AgentLocation &uccLoc, unsigned int searchDepth, double distance_traveled);
	~UCP();

	// Agent methods
	void update() override;
	void stop() override;
	UCP* asUCP() override { return this; }
	void OnPacketReceived(TCPSocketPtr socket, const PacketHeader &packetHeader, InputMemoryStream &stream) override;

	// TODO

	double TraveledDistance() const { return distance_traveled; }

	// Whether or not the negotiation finished
	bool negotiationFinished() const;

	// Whether ir not there was a negotiation agreement
	bool negotiationAgreement() const;

private:

	// UCP data
	uint16_t _requestedItemId; /**< The item to request. */
	uint16_t _contributedItemId;
	uint16_t _constraintUCCItemId;
	AgentLocation _uccLocation; /**< Location of the remote UCC agent. */
	unsigned int searchDepth = 0;
	double distance_traveled = 0;

	// MCP
	MCPPtr _mcp; /**< The child MCP. */
	void createChildMCP(uint16_t constraintItemId);
	void destroyChildMCP();

	// Agreement
	bool _negotiationAgreement; /**< Was there a negotiation agreement? */
};

