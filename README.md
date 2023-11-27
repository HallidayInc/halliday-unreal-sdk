<p align="center">
<br />
<a href="https://halliday.xyz"><img src="https://github.com/HallidayInc/UnitySDK/blob/master/hallidayLogo.svg" width="100" alt=""/></a>
</p>
<h1 align="center">Halliday Unreal Engine SDK</h1>

# Installation

Download the latest source code from the [releases](https://github.com/HallidayInc/halliday-unreal-sdk/releases) page.

Drag and drop the contents, e.i. the **HallidaySDK** and **Web3AuthSDK** folders, from the source code into the **Plugins** directory of your project.


# Build

Click on your **.uproject** file to open Unreal Engine and select **Yes** when you get this message: **"Missing <Your Project> Modules [...] Would you like to rebuild them now?"**.

# Usage
You can access the HallidaySDK through C++ or Blueprints. The HallidaySDK comes with an example found in the **HallidaySDK Content** folder where you will find two assets: **HallidaySDKTest** and **Web3AuthInstance**. Drag and drop these two into your game and update the event graph of the LoginWidget and ApiWidget with your public API key and press play.

All SDK responses are handled with delegates. You can bind an event in C++ or in Blueprints (as shown in the ApiWidget event graph).

```cpp
// Get the Halliday Component
AHalliday* HallidayClient = Cast<AHalliday>(UGameplayStatics::GetActorOfClass(GetWorld(), AHalliday::StaticClass()));

// Initialize the component with your public Halliday API Key
HallidayClient.Initialize("INSERT_HALLIDAY_PUBLIC_API_KEY", EBlockchainType::MUMBAI, true, "INSERT_YOUR_CLIENT_VERIFIER_ID");

// Get your player's wallets
// If your player is new, the HallidayClient will create a new wallet and return it.
// If this is a returning player, HallidayClient will return the wallet stored for the player.
HallidayClient->GetOrCreateHallidayAAWallet(InGamePlayerId);

// Get Player NFTs
HallidayClient->GetAssets(InGamePlayerId);

// Get Player ERC-20 and Native Token balances
HallidayClient->GetBalances(InGamePlayerId);

// Call BalanceTransfer to transfer native tokens
HallidayClient->TransferBalance(
    from_in_game_player_id: PlayerIdOne,
    to_in_game_player_id: PlayerIdTwo,
    blockchain_type: EBlockchainType::MUMBAI,
    sponsor_gas: true,
    value: "0.025"
);

// Call BalanceTransfer to transfer ERC20 tokens
HallidayClient->TransferBalance(
    from_in_game_player_id: PlayerIdOne,
    to_in_game_player_id: PlayerIdTwo,
    blockchain_type: EBlockchainType::MUMBAI,
    sponsor_gas: true,
    value: "1000000000000000000", // 1 ETH
    token_address: ERC20TokenAddress
);

// Send NFT from one player to another
HallidayClient->TransferAsset(
    from_in_game_player_id: PlayerIdOne,
    to_in_game_player_id: PlayerIdTwo,
    collection_address: ERC721ContractAddress,
    token_id: ERC721TokenId,
    blockchain_type: EBlockchainType::MUMBAI,
    sponsor_gas: true
);

// Make Contract Call
HallidayClient->CallContract(
    from_in_game_player_id: PlayerIdOne,
    target_address: ERC20ContractAddress,
    calldata: Calldata,
    value: "0",
    blockchain_type: EBlockchainType::MUMBAI,
    sponsor_gas: true
);
```
