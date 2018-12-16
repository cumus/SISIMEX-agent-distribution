#include "MCP.h"
#include "UCP.h"
#include "Application.h"
#include "ModuleAgentContainer.h"


enum State
{
	ST_MCP_INIT,
	ST_MCP_REQUESTING_MCCs,
	ST_MCP_ITERATING_OVER_MCCs,
	ST_MCP_STARTING_NEGOTIATION,
	ST_MCP_NEGOTIATING,
	ST_MCP_NEGOTIATION_FINISHED
};

MCP::MCP(Node *node, uint16_t requestedItemID, uint16_t contributedItemID, unsigned int searchDepth) :
	Agent(node),
	_requestedItemId(requestedItemID),
	_contributedItemId(contributedItemID),
	_searchDepth(searchDepth)
{
	setState(ST_MCP_INIT);
}

MCP::~MCP()
{
}

void MCP::update()
{
	switch (state())
	{
	case ST_MCP_INIT:
		queryMCCsForItem(_requestedItemId);
		setState(ST_MCP_REQUESTING_MCCs);
		break;

	case ST_MCP_ITERATING_OVER_MCCs:
		// TODO: Handle this state
		if (_mccRegisterIndex < _mccRegisters.size())
		{
			const AgentLocation &mccRegister(_mccRegisters[_mccRegisterIndex]);
			sendNegotiationRequest(mccRegister);
			setState(ST_MCP_STARTING_NEGOTIATION);
		}
		else
		{
			setState(ST_MCP_NEGOTIATION_FINISHED);
		}
		break;

	// TODO: Handle other states
	case ST_MCP_NEGOTIATING:
		if (_ucp->negotiationFinished()) {
			if (_ucp->negotiationAgreement()) {
				_negotiationAgreement = true;
				setState(ST_MCP_NEGOTIATION_FINISHED);
			}
			else {
				_mccRegisterIndex++;
				setState(ST_MCP_ITERATING_OVER_MCCs);
			}
			destroyChildUCP();
		}
		break;

	default:;
	}
}

void MCP::stop()
{
	// TODO: Destroy the underlying search hierarchy (UCP->MCP->UCP->...)
	destroyChildUCP();
	destroy();
}

void MCP::OnPacketReceived(TCPSocketPtr socket, const PacketHeader &packetHeader, InputMemoryStream &stream)
{
	const PacketType packetType = packetHeader.packetType;

	switch (packetType)
	{
	case PacketType::ReturnMCCsForItem:
	{
		if (state() == ST_MCP_REQUESTING_MCCs)
		{
			// Read the packet
			PacketReturnMCCsForItem packetData;
			packetData.Read(stream);

			// Log the returned MCCs
			for (auto &mccdata : packetData.mccAddresses)
			{
				uint16_t agentId = mccdata.agentId;
				const std::string &hostIp = mccdata.hostIP;
				uint16_t hostPort = mccdata.hostPort;
				//iLog << " - MCC: " << agentId << " - host: " << hostIp << ":" << hostPort;
			}

			// Store the returned MCCs from YP
			_mccRegisters.swap(packetData.mccAddresses);

			// Select the first MCC to negociate
			_mccRegisterIndex = 0;
			setState(ST_MCP_ITERATING_OVER_MCCs);

			socket->Disconnect();
		}
		else
		{
			wLog << "OnPacketReceived() - PacketType::ReturnMCCsForItem was unexpected.";
		}
		break;
	}

	// TODO: Handle other packets
	case PacketType::NegociationProposalAnswer:
	{
		if (state() == ST_MCP_STARTING_NEGOTIATION)
		{
			PacketStartNegotiationResponse iPacketData;
			iPacketData.Read(stream);

			// Create UCP to achieve the constraint item
			AgentLocation uccLoc;
			uccLoc.hostIP = socket->RemoteAddress().GetIPString();
			uccLoc.hostPort = LISTEN_PORT_AGENTS;
			uccLoc.agentId = iPacketData.uccAgentId;
			createChildUCP(uccLoc);

			// Wait for UCP results
			setState(ST_MCP_NEGOTIATING);

			socket->Disconnect();
		}
		else
		{
			wLog << "OnPacketReceived() - PacketType::NegociationProposalAnswer was unexpected.";
		}
		break;
	}
	default:
		wLog << "OnPacketReceived() - Unexpected PacketType.";
	}
}

bool MCP::negotiationFinished() const
{
	return state() == ST_MCP_NEGOTIATION_FINISHED;
}

bool MCP::negotiationAgreement() const
{
	return _negotiationAgreement; // TODO: Did the child UCP find a solution?
}


bool MCP::queryMCCsForItem(int itemId)
{
	// Create message header and data
	PacketHeader packetHead;
	packetHead.packetType = PacketType::QueryMCCsForItem;
	packetHead.srcAgentId = id();
	packetHead.dstAgentId = -1;
	PacketQueryMCCsForItem packetData;
	packetData.itemId = _requestedItemId;

	// Serialize message
	OutputMemoryStream stream;
	packetHead.Write(stream);
	packetData.Write(stream);

	// 1) Ask YP for MCC hosting the item 'itemId'
	return sendPacketToYellowPages(stream);
}

bool MCP::sendNegotiationRequest(const AgentLocation &mccRegister)
{
	const std::string &hostIP = mccRegister.hostIP;
	const uint16_t hostPort = mccRegister.hostPort;
	const uint16_t agentId = mccRegister.agentId;

	PacketHeader packetHead;
	packetHead.packetType = PacketType::NegociationProposalRequest;
	packetHead.srcAgentId = id();
	packetHead.dstAgentId = mccRegister.agentId;

	OutputMemoryStream stream;
	packetHead.Write(stream);

	return sendPacketToAgent(hostIP, hostPort, stream);
}

void MCP::createChildUCP(const AgentLocation &uccLoc)
{
	destroyChildUCP(); // just in case
	_ucp = App->agentContainer->createUCP(node(), _requestedItemId, _contributedItemId, uccLoc, _searchDepth);
}

void MCP::destroyChildUCP()
{
	if (_ucp.get()) {
		_ucp->stop();
		_ucp.reset();
	}
}

