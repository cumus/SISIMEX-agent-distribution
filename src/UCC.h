#pragma once
#include "Agent.h"

class UCC :
	public Agent
{
public:

	// Constructor and destructor
	UCC(Node *node, uint16_t contributedItemId, uint16_t constraintItemId);
	~UCC();

	// Agent methods
	void update() override { }
	void stop() override;
	UCC* asUCC() override { return this; }
	void OnPacketReceived(TCPSocketPtr socket, const PacketHeader &packetHeader, InputMemoryStream &stream) override;

	// TODO
	
	// Whether or not the negotiation finished
	bool negotiationFinished() const;

	// Whether ir not there was a negotiation agreement
	bool negotiationAgreement() const;

private:

	uint16_t _contributedItemId; /**< The contributed item. */
	uint16_t _constraintItemId; /**< The constraint item. */

	bool _negotiationAgreement; /**< Was there a negotiation agreement? */

};

