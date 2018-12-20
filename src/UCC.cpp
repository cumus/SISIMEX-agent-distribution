#include "UCC.h"

// TODO: Make an enum with the states
enum State
{
	ST_UCC_WAITING_ITEM_REQUEST,
	ST_UCC_WAITING_ITEM_CONSTRAINT,

	ST_UCC_NEGOTIATION_FINISHED
};

UCC::UCC(Node *node, uint16_t contributedItemId, uint16_t constraintItemId) :
	Agent(node),
	_contributedItemId(contributedItemId),
	_constraintItemId(constraintItemId),
	_negotiationAgreement(false)
{
	// TODO: Save input parameters
	setState(ST_UCC_WAITING_ITEM_REQUEST);
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
		if (state() == ST_UCC_WAITING_ITEM_REQUEST)
		{
			// Send back PacketType::RequestItemResponse with the constraint
			PacketHeader oPacketHead;
			oPacketHead.packetType = PacketType::RequestItemResponse;
			oPacketHead.srcAgentId = id();
			oPacketHead.dstAgentId = packetHeader.srcAgentId;

			/* Do nothing with item requested
			PacketRequestItem iPacketData;
			iPacketData.Read(stream);*/

			PacketRequestItemResponse oPacketData;
			oPacketData.constraintItemId = _constraintItemId;

			OutputMemoryStream ostream;
			oPacketHead.Write(ostream);
			oPacketData.Write(ostream);

			socket->SendPacket(ostream.GetBufferPtr(), ostream.GetSize());
			setState(ST_UCC_WAITING_ITEM_CONSTRAINT);
		}
		else
		{
			wLog << "OnPacketReceived() - PacketType::RequestItem was unexpected.";
		}
		break;
	}
	case PacketType::SendConstraint:
	{
		if (state() == ST_UCC_WAITING_ITEM_CONSTRAINT)
		{
			PacketSendConstraint iPacketData;
			iPacketData.Read(stream);
			_negotiationAgreement = iPacketData.agreement;

			/* Do nothing with item recieved
			iPacketData.constraintItemId;*/

			PacketHeader oPacketHead;
			oPacketHead.packetType = PacketType::SendConstraintResponse;
			oPacketHead.srcAgentId = id();
			oPacketHead.dstAgentId = packetHeader.srcAgentId;
			
			OutputMemoryStream ostream;
			oPacketHead.Write(ostream);

			socket->SendPacket(ostream.GetBufferPtr(), ostream.GetSize());
			setState(ST_UCC_NEGOTIATION_FINISHED);
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
	return state() == ST_UCC_NEGOTIATION_FINISHED;
}

bool UCC::negotiationAgreement() const
{
	return _negotiationAgreement;
}
