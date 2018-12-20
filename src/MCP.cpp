#include "MCP.h"
#include "UCP.h"
#include "Application.h"
#include "ModuleAgentContainer.h"
#include "ModuleNodeCluster.h"


enum State
{
	ST_MCP_INIT,
	ST_MCP_REQUESTING_MCCs,
	ST_MCP_MCC_POSITION_REQUEST,
	ST_MCP_MCC_POSITION_RESPONSE,
	ST_MCP_ITERATING_OVER_MCCs,
	ST_MCP_WAITING_NEGOTIATION_RESPONSE,
	ST_MCP_NEGOTIATING,
	ST_MCP_NEGOTIATION_FINISHED
};

MCP::MCP(Node *node, uint16_t requestedItemID, uint16_t contributedItemID, unsigned int searchDepth, double distance_traveled) :
	Agent(node),
	_requestedItemId(requestedItemID),
	_contributedItemId(contributedItemID),
	_searchDepth(searchDepth),
	distance_traveled(distance_traveled),
	_mccRegisterIndex(false)
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
		ordered_distances.clear();
		queryMCCsForItem(_requestedItemId);
		setState(ST_MCP_REQUESTING_MCCs);
		break;

	case ST_MCP_MCC_POSITION_REQUEST:
		if (_mccRegisterIndex < _mccRegisters.size())
		{
			const AgentLocation &agent(_mccRegisters[_mccRegisterIndex]);

			PacketHeader packetHead;
			packetHead.packetType = PacketType::PositionRequest;
			packetHead.srcAgentId = id();
			packetHead.dstAgentId = agent.agentId;

			OutputMemoryStream stream;
			packetHead.Write(stream);

			sendPacketToAgent(agent.hostIP, agent.hostPort, stream);
			setState(ST_MCP_MCC_POSITION_RESPONSE);
		}
		else
		{
			_mccRegisterIndex = 0;
			setState(ST_MCP_ITERATING_OVER_MCCs);
		}
		break;
	case ST_MCP_ITERATING_OVER_MCCs:
		// TODO: Handle this state
		if (_mccRegisterIndex < ordered_distances.size()
			&& _mccRegisterIndex < App->modNodeCluster->MaxNearest())
		{
			const AgentLocation &agent(_mccRegisters[ordered_distances[_mccRegisterIndex].first]);

			PacketHeader packetHead;
			packetHead.packetType = PacketType::NegociationProposalRequest;
			packetHead.srcAgentId = id();
			packetHead.dstAgentId = agent.agentId;

			OutputMemoryStream stream;
			packetHead.Write(stream);

			sendPacketToAgent(agent.hostIP, agent.hostPort, stream);
			setState(ST_MCP_WAITING_NEGOTIATION_RESPONSE);
		}
		else
		{
			setState(ST_MCP_NEGOTIATION_FINISHED);
			destroyChildUCP();
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

			// Store the returned MCCs from YP
			_mccRegisters.swap(packetData.mccAddresses);

			// Select the first MCC to negociate
			_mccRegisterIndex = 0;
			setState(ST_MCP_MCC_POSITION_REQUEST);

			socket->Disconnect();
		}
		else
		{
			wLog << "OnPacketReceived() - PacketType::ReturnMCCsForItem was unexpected.";
		}
		break;
	}
	case PacketType::PositionAnswer:
	{
		if (state() == ST_MCP_MCC_POSITION_RESPONSE)
		{
			// Read the packet
			PacketPositionResponse packetData;
			packetData.Read(stream);

			double distance = sqrt(pow(node()->x() - packetData.x, 2) + pow(node()->y() - packetData.y, 2));

			if (distance + distance_traveled <= App->modNodeCluster->MaxTravelDistance())
			{
				bool ordered = false;

				for (auto agent = ordered_distances.begin(); !ordered && agent != ordered_distances.end();)
				{
					if (distance <= agent->second)
					{
						ordered = true;
						ordered_distances.insert(agent, *new std::pair<int, double>(_mccRegisterIndex, distance));
					}
					else
					{
						agent++;
					}
				}

				if (!ordered)
					ordered_distances.push_back(*new std::pair<int, double>(_mccRegisterIndex, distance));
			}

			_mccRegisterIndex++;
			setState(ST_MCP_MCC_POSITION_REQUEST);
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
		if (state() == ST_MCP_WAITING_NEGOTIATION_RESPONSE)
		{
			PacketStartNegotiationResponse iPacketData;
			iPacketData.Read(stream);

			if (iPacketData.negociation_approved)
			{
				// Create UCP to achieve the constraint item
				createChildUCP(iPacketData.ucc_location);

				// Wait for UCP results
				setState(ST_MCP_NEGOTIATING);
			}
			else
			{
				_mccRegisterIndex++;
				setState(ST_MCP_ITERATING_OVER_MCCs);
			}

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

void MCP::createChildUCP(const AgentLocation &uccLoc)
{
	_ucp.reset();
	_ucp = App->agentContainer->createUCP(node(), _requestedItemId, _contributedItemId, uccLoc, _searchDepth, distance_traveled + ordered_distances[_mccRegisterIndex].second);
}

void MCP::destroyChildUCP()
{
	if (_ucp.get()) {
		_ucp->stop();
		_ucp.reset();
	}
}

