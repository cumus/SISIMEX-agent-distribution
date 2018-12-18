#pragma once
#include "Agent.h"

// Forward declaration
class UCC;
using UCCPtr = std::shared_ptr<UCC>;

class MCC :
	public Agent
{
public:

	// Constructor and destructor
	MCC(Node *node, uint16_t contributedItemId, uint16_t constraintItemId);
	~MCC();

	// Agent methods
	void update() override;
	void stop() override;
	MCC* asMCC() override { return this; }
	void OnPacketReceived(TCPSocketPtr socket, const PacketHeader &packetHeader, InputMemoryStream &stream) override;

	// Getters
	bool isIdling() const;
	uint16_t contributedItemId() const { return _contributedItemId; }
	uint16_t constraintItemId() const { return _constraintItemId; }

	// Whether or not the negotiation finished
	bool negotiationFinished() const;

	// Whether or not there was a negotiation agreement
	bool negotiationAgreement() const;

	// Whether or not a negociation is in course
	bool CurrentlyNegotiating() const;

private:

	uint16_t _contributedItemId; /**< The contributed item. */
	uint16_t _constraintItemId; /**< The constraint item. */

	bool registerIntoYellowPages();
	void unregisterFromYellowPages();

	// UCC
	UCCPtr _ucc;
	void createChildUCC();
	void destroyChildUCC();

	bool negociation_succesful = false;
};
