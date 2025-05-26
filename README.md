# Crypto Orderbook Creation Project

A comprehensive cryptocurrency orderbook implementation that aggregates real-time market data from multiple major exchanges using both REST API and WebSocket connections.

## Overview

This project creates a unified orderbook system that connects to three major cryptocurrency exchanges:
- **Coinbase Pro** (now Coinbase Advanced Trade)
- **Binance**
- **Kraken**

The implementation follows a two-phase approach: starting with REST API endpoints for initial data retrieval, then establishing WebSocket connections for real-time updates.

## Architecture

### Phase 1: REST API Foundation

We began by implementing REST API connections to establish the basic orderbook structure:

- **Initial Snapshot Retrieval**: Fetch current orderbook state from each exchange
- **Data Normalization**: Standardize different exchange formats into a unified structure
- **Error Handling**: Implement robust error handling for API rate limits and failures
- **Testing Framework**: Validate data integrity and API response handling

### Phase 2: WebSocket Real-time Updates

After establishing the REST foundation, we implemented WebSocket connections for live data:

- **Real-time Streaming**: Subscribe to live orderbook updates
- **Incremental Updates**: Process bid/ask changes efficiently
- **Connection Management**: Handle reconnections, heartbeats, and error recovery
- **Data Synchronization**: Maintain orderbook accuracy with periodic REST snapshots


We made realtime orderbooks for Coinbase, Binance, and Kraken. We started with Binance, then Kraken, and then after noticing a significant lag in the orderbook updates, we switched to Coinbase. we realized that Coinbase was the most reliable and accurate orderbook. due to the Level 2 orderbook updates we were able to reeieve from it. The Coinbase connection was also made through websockets instead of the REST API.

The orderbook creation was done in C++ and the websocket connection was done in C++ as well. The priority is FIFO, with a price and time priority.