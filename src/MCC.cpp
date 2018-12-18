#include "MCC.h"
#include "UCC.h"
#include "Application.h"
#include "ModuleAgentContainer.h"


enum State
{
	ST_MCC_INIT,
	ST_MCC_REGISTERING,
	ST_MCC_IDLE,

	ST_MCC_NEGOTIATING,
	ST_MCC_NEGOTIATION_FINISHED,
	ST_MCC_UNREGISTERING,

	ST_MCC_FINISHED
};

MCC::MCC(Node *node, uint16_t contributedItemId, uint16_t constraintItemId) :
	Agent(node),
	_contributedItemId(contributedItemId),
	_constraintItemId(constraintItemId)
{
	setState(ST_MCC_INIT);
}


MCC::~MCC()
{
}

void MCC::update()
{
	switch (state())
	{
	case ST_MCC_INIT:
		if (registerIntoYellowPages()) {
			setState(ST_MCC_REGISTERING);
		}
		else {
			setState(ST_MCC_FINISHED);
		}
		break;

	case ST_MCC_REGISTERING:
		// See OnPacketReceived()
		break;

		// TODO: Handle other states
	case ST_MCC_NEGOTIATING:
		if (_ucc->negotiationFinished()) {
			if (_ucc->negotiationAgreement())
			{
				negociation_succesful = true;
				setState(ST_MCC_NEGOTIATION_FINISHED);
			}
			else {
				setState(ST_MCC_IDLE);
			}
			destroyChildUCC();
		}
		break;
	case ST_MCC_UNREGISTERING:
		// See OnPacketReceived()
		break;
	case ST_MCC_FINISHED:
		destroy();
	}
}

void MCC::stop()
{
	// Destroy hierarchy below this agent (only a UCC, actually)
	destroyChildUCC();

	unregisterFromYellowPages();
	setState(ST_MCC_FINISHED);
}


void MCC::OnPacketReceived(TCPSocketPtr socket, const PacketHeader &packetHeader, InputMemoryStream &stream)
{
	const PacketType packetType = packetHeader.packetType;

	switch (packetType)
	{
	case PacketType::RegisterMCCAck:
	{
		if (state() == ST_MCC_REGISTERING)
		{
			setState(ST_MCC_IDLE);
			socket->Disconnect();
		}
		else
		{
			wLog << "OnPacketReceived() - PacketType::RegisterMCCAck was unexpected.";
		}
		break;
	}
	// TODO: Handle other packets
	case PacketType::NegociationProposalRequest:
	{
		if (state() == ST_MCC_IDLE || CurrentlyNegotiating())
		{
			// Send negotiation response
			PacketHeader oPacketHead;
			oPacketHead.packetType = PacketType::NegociationProposalAnswer;
			oPacketHead.srcAgentId = id();
			oPacketHead.dstAgentId = packetHeader.srcAgentId;

			PacketStartNegotiationResponse oPacketData;
			oPacketData.negociation_approved = false;

			if (!CurrentlyNegotiating())// && VALID)
			{
				// Create UCC
				createChildUCC();

				AgentLocation ucclocation;
				ucclocation.hostIP = socket->RemoteAddress().GetIPString();
				ucclocation.agentId = _ucc->id();
				ucclocation.hostPort = LISTEN_PORT_AGENTS;

				oPacketData.ucc_location = ucclocation;
				oPacketData.negociation_approved = true;

				negociation_succesful = false;
				setState(ST_MCC_NEGOTIATING);
			}

			OutputMemoryStream ostream;
			oPacketHead.Write(ostream);
			oPacketData.Write(ostream);

			socket->SendPacket(ostream.GetBufferPtr(), ostream.GetSize());
		}
		else
		{
			wLog << "OnPacketReceived() - PacketType::NegociationProposalRequest was unexpected.";
		}
		break;
	}

	default:
		wLog << "OnPacketReceived() - Unexpected PacketType.";
	}
}

bool MCC::isIdling() const
{
	return state() == ST_MCC_IDLE;
}

bool MCC::negotiationFinished() const
{
	return state() == ST_MCC_FINISHED;
}

bool MCC::negotiationAgreement() const
{
	// If this agent finished, means that it was an agreement
	// Otherwise, it would return to state ST_IDLE
	return negociation_succesful; // negotiationFinished();
}

bool MCC::CurrentlyNegotiating() const
{
	return state() > ST_MCC_IDLE && state() < ST_MCC_FINISHED;
}

bool MCC::registerIntoYellowPages()
{
	// Create message header and data
	PacketHeader packetHead;
	packetHead.packetType = PacketType::RegisterMCC;
	packetHead.srcAgentId = id();
	packetHead.dstAgentId = -1;
	PacketRegisterMCC packetData;
	packetData.itemId = _contributedItemId;

	// Serialize message
	OutputMemoryStream stream;
	packetHead.Write(stream);
	packetData.Write(stream);

	return sendPacketToYellowPages(stream);
}

void MCC::unregisterFromYellowPages()
{
	// Create message
	PacketHeader packetHead;
	packetHead.packetType = PacketType::UnregisterMCC;
	packetHead.srcAgentId = id();
	packetHead.dstAgentId = -1;
	PacketUnregisterMCC packetData;
	packetData.itemId = _contributedItemId;

	// Serialize message
	OutputMemoryStream stream;
	packetHead.Write(stream);
	packetData.Write(stream);

	sendPacketToYellowPages(stream);
}

void MCC::createChildUCC()
{
	// TODO: Create a unicast contributor
	_ucc.reset();
	_ucc = App->agentContainer->createUCC(node(), _contributedItemId, _constraintItemId);
}

void MCC::destroyChildUCC()
{
	// TODO: Destroy the unicast contributor child
	if (_ucc.get()) {
		_ucc->stop();
		_ucc.reset();
	}
}
