#include "UCC.h"


// TODO: Make an enum with the states
enum State
{
	ST_WAITING_ITEM_REQUEST,
	ST_WAITING_ITEM_CONSTRAINT,
	ST_NEGOTIATION_FINISHED
};

UCC::UCC(Node *node, uint16_t contributedItemId, uint16_t constraintItemId) :
	Agent(node),
	_contributedItemId(contributedItemId),
	_constraintItemId(constraintItemId),
	_negotiationAgreement(false)
{
	// TODO: Save input parameters
	setState(ST_WAITING_ITEM_REQUEST);
}

UCC::~UCC()
{
}

void UCC::stop()
{
	destroy();
}

void UCC::OnPacketReceived(TCPSocketPtr socket, const PacketHeader &packetHeader, InputMemoryStream &stream)
{
	PacketType packetType = packetHeader.packetType;

	switch (packetType)
	{
		// TODO: Handle packets
	case PacketType::RequestItem:
	{
		if (state() == ST_WAITING_ITEM_REQUEST)
		{
			PacketRequestItem iPacketData;
			iPacketData.Read(stream);
			if (iPacketData.requestedItemId != _contributedItemId) {
				// Should send error...
			}

			// Send back PacketType::RequestItemResponse with the constraint
			PacketHeader oPacketHead;
			oPacketHead.packetType = PacketType::RequestItemResponse;
			oPacketHead.srcAgentId = id();
			oPacketHead.dstAgentId = packetHeader.srcAgentId;
			PacketRequestItemResponse oPacketData;
			oPacketData.constraintItemId = _constraintItemId;
			OutputMemoryStream ostream;
			oPacketHead.Write(ostream);
			oPacketData.Write(ostream);
			socket->SendPacket(ostream.GetBufferPtr(), ostream.GetSize());

			if (_constraintItemId == NULL_ITEM_ID) {
				_negotiationAgreement = true;
				setState(ST_NEGOTIATION_FINISHED);
			}
			else {
				setState(ST_WAITING_ITEM_CONSTRAINT);
			}
		}
		else
		{
			wLog << "OnPacketReceived() - PacketType::RequestItem was unexpected.";
		}
		break;
	}
	case PacketType::SendConstraint:
	{
		if (state() == ST_WAITING_ITEM_CONSTRAINT)
		{
			PacketSendConstraint iPacketData;
			iPacketData.Read(stream);
			if (iPacketData.constraintItemId != _constraintItemId) {
				// Should send an error...
			}

			// Send back PacketType::RequestItemResponse with the constraint
			PacketHeader oPacketHead;
			oPacketHead.packetType = PacketType::RequestItemResponse;
			oPacketHead.srcAgentId = id();
			oPacketHead.dstAgentId = packetHeader.srcAgentId;
			OutputMemoryStream ostream;
			oPacketHead.Write(ostream);
			socket->SendPacket(ostream.GetBufferPtr(), ostream.GetSize());

			_negotiationAgreement = true;
			setState(ST_NEGOTIATION_FINISHED);
		}
		else
		{
			wLog << "OnPacketReceived() - PacketType::SendConstraint was unexpected.";
		}
		break;
	}

	default:
		wLog << "OnPacketReceived() - Unexpected PacketType.";
	}
}

bool UCC::negotiationFinished() const
{
	return state() == ST_NEGOTIATION_FINISHED;
}

bool UCC::negotiationAgreement() const
{
	return _negotiationAgreement;
}
