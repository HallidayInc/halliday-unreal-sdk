#pragma once

#include "CoreMinimal.h"
#include "Web3Auth.h"
#include "Http.h"
#include "GameFramework/Actor.h"
#include "HallidayTypes.h"

#include "Halliday.generated.h"

/** Bind a callback function to this delegate if you want to execute an action after the player has logged in. */
DECLARE_DYNAMIC_DELEGATE(FOnLoginCompleted);
/** Bind a callback function to this delegate if you want to execute an action after the player has logged out. */
DECLARE_DYNAMIC_DELEGATE(FOnLogoutCompleted);
/** Bind a callback function to this delegate to receive a response after your client has called  GetOrCreateHallidayAAWallet() */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWalletReceived, FWallet, Wallet);
/** Bind a callback function to this delegate to receive a response after your client has called GetAssets() */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAssetsReceived, FGetAssetsResponse, GetAssetsReponse);
/** Bind a callback function to this delegate to receive a response after your client has called GetBalances() */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBalancesReceived, FGetBalancesResponse, GetBalancesReponse);
/** Bind a callback function to this delegate to receive a response after your client has called GetTransaction() */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransactionReceived, FGetTransactionResponse, GetTransactionReponse);
/** Bind a callback function to this delegate to receive a response after your transaction has been submitted in TransferAsset() */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransferAssetSubmitted, FSubmitTransactionResponse, SubmitTransactionResponse);
/** Bind a callback function to this delegate to receive a response after your transaction has been submitted in TransferBalance() */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransferBalanceSubmitted, FSubmitTransactionResponse, SubmitTransactionResponse);
/** Bind a callback function to this delegate to receive a response after your transaction has been submitted in CallContract() */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCallContractSubmitted, FSubmitTransactionResponse, SubmitTransactionResponse);
/** Bind a callback function to this delegate to receive a respone after your client has called BuildCalldata() */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCalldataBuilt, FBuildCalldataResponse, BuildCalldataResponse);

UCLASS()
class HALLIDAYSDK_API AHalliday : public AActor
{
	GENERATED_BODY()
    
protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

public:
    // Called every frame
    virtual void Tick(float DeltaTime) override;
    
    /** Deletegates for you to bind callbacks to */
    FOnLoginCompleted OnLoginCompleted;
    FOnLogoutCompleted OnLogoutCompleted;
    
    UPROPERTY(BlueprintAssignable, Category = "Halliday")
        FOnWalletReceived OnWalletReceived;
    
    UPROPERTY(BlueprintAssignable, Category = "Halliday")
        FOnAssetsReceived OnAssetsReceived;
    
    UPROPERTY(BlueprintAssignable, Category = "Halliday")
        FOnBalancesReceived OnBalancesReceived;
    
    UPROPERTY(BlueprintAssignable, Category = "Halliday")
        FOnTransactionReceived OnTransactionReceived;
    
    UPROPERTY(BlueprintAssignable, Category = "Halliday")
        FOnTransferAssetSubmitted OnTransferAssetSubmitted;
    
    UPROPERTY(BlueprintAssignable, Category = "Halliday")
        FOnTransferBalanceSubmitted OnTransferBalanceSubmitted;
    
    UPROPERTY(BlueprintAssignable, Category = "Halliday");
        FOnCallContractSubmitted OnCallContractSubmitted;
    
	AHalliday();
    
    /**
     * Call this function to begin using the client methods.
     * @param PublicApiKey - Your Halliday public API key.
     * @param EBlockchainType - The blockchain type of your application.
     * @param bIsSandbox - Sandbox or production mode. True by default.
     * @warning Call this at least one before using any other client method.
     */
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        void Initialize(const FString& PublicApiKey, EBlockchainType BlockchainType = EBlockchainType::MUMBAI, bool bIsSandbox = true, const FString& ClientVerifierId = "");
                     
    /**
     * Call this function to open a Google login page in another window.
     */
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        void LogInWithGoogle();
    
    /**
     * Call this function to open a Facebook login page in another window.
     */
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        void LogInWithFacebook();

    /**
     * Call this function login in with email.
     */
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        void LogInWithEmailOtp(const FString& EmailAddress);
    
    /**
     * Pass in a callback function that you want to execute after a user has logged in.
     * @param Callback Your callback function to execute upon login completion.
     */
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        void SetLoginCompletedCallback(FOnLoginCompleted Callback);
    
    /**
     * Pass in a callback function that you want to execute after a user has logged out.
     * @param Callback Your callback function to execute upon logout completion.
     */
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        void SetLogoutCompletedCallback(FOnLogoutCompleted Callback);

    /**
     * Get your player's account abstraction wallet address.
     * @param InGamePlayerId Id of the player you want to fetch an address for.
     * @param bWasPreviouslyCalled Internally used. You do not need to use this parameter.
     */
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        void GetOrCreateHallidayAAWallet(const FString& InGamePlayerId, bool bWasPreviouslyCalled = false);
        
    /**
     * Get your player's assets.
     * @param InGamePlayerId Id of the player  you want to fetch assets for,
     */
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        void GetAssets(const FString& InGamePlayerId);
    
    /**
     * Get your players' native and ERC20 token balances.
     * @param InGamePlayerId Id of the player you want to fetch token balances for,
     */
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        void GetBalances(const FString& InGamePlayerId);
    
    /**
     * Get a transaction that your player has built or executed.
     * @param TxId Id of the transaction id you want to fetch.
     */
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        void GetTransaction(const FString& TxId);
    
    /**
     * Enable your player to transfer an asset to another within your application.
     * @param FromInGamePlayerId Id of the player who is sending an asset.
     * @param ToInGamePlayerId Id of the player who is receiving an asset.
     * @param CollectionAddress Contract address of the asset.
     * @param TokenId Id of the asset.
     * @param BlockchainType Blockchain where the asset resides.
     * @param bSponsorGas Enable gas sponsorship for this transaction.
     * @warning Gas sponsorship is disabled by default. Please reach out to the Halliday team to enable gas sponsorship for your application.
     */
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        void TransferAsset(const FString& FromInGamePlayerId, const FString& ToInGamePlayerId, const FString& CollectionAddress, const FString& TokenId, EBlockchainType BlockchainType, bool bSponsorGas);
    
    /**
     * Enable your player to transfer native or ERC20 tokens  to another within your application.
     * @param FromInGamePlayerId Id of the player who is sending tokens.
     * @param ToInGamePlayerId Id of the player who is receiving tokens.
     * @param BlockchainType Blockchain where the tokens reside.
     * @param bSponsorGas Enable gas sponsorship for this transaction.
     * @param Value The amount of token to send. Please check and use the decimals of the token, e.i. 1 ETH = 1e18.
     * @param TokenAddress [Optional for native transfers] Address of the token contract.
     * @warning Gas sponsorship is disabled by default. Please reach out to the Halliday team to enable gas sponsorship for your application.
     */
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        void TransferBalance(const FString& FromInGamePlayerId, const FString& ToInGamePlayerId, EBlockchainType BlockchainType, bool bSponsorGas, const FString& Value, const FString& TokenAddress = "");
    
    /**
     * Enable your player to call any contract call with their Halliday smart account.
     * This enables you as the developer to create any onchain functionality for your players to use.
     * @param FromInGamePlayerId Id of the player who is sending tokens.
     * @param TargetAddress Address of the contract.
     * @param Calldata Calldata of the onchain function that you want to execute.
     * @param BlockchainType Blockchain where the contract call is made.
     * @param bSponsorGas Enable gas sponsorship for this transaction.
     * @param Value [Optional] The amount for native transfer calls. Please check and use the decimals of the token, e.i. 1 ETH = 1e18.
     * @warning Gas sponsorship is disabled by default. Please reach out to the Halliday team to enable gas sponsorship for your application.
     */
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        void ContractCall(const FString& FromInGamePlayerId, const FString& TargetAddress, const FString& Calldata, EBlockchainType BlockchainType, bool bSponsorGas, const FString& Value = "0");
    
    /**
     * Getters and setters.
     */
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        AWeb3Auth* GetWeb3Auth();
    
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        EBlockchainType GetBlockchainType();
    
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        FString GetAuthHeaderValue();
    
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        FString GetApiEndpoint();
    
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        FString GetInGamePlayerId();
    
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        FUserInfo GetUserInfo();
    
    UFUNCTION(BlueprintCallable, Category = "Halliday")
        void SetInGamePlayerId(const FString& InGamePlayerId);

    /**
     * Generate the public key using the private key returned by Web3Auth.
     * You do not need to call this.
     */
    UFUNCTION()
        FString _GetPublicKeyFromPrivateKey();
    
    /**
     * Sign a transaction hash with a private key.
     * You do not need to call this.
     * @param TxHash Keccak256 hashed transaction hash
     * @param PrivateKey Private key to sign with
     * @returns A signature
     */
    UFUNCTION()
        FString _Secp256k1(FString TxHash);
private:
    /**
     * [PRIVATE HELPER METHODS] You will not need to call these yourself.
     */
    
    /**
     * Callback function that Halliday will execute after the user has logged in to initialize member variables with the response from Web3 Auth.
     */
    UFUNCTION()
        void _HandleLogin(FWeb3AuthResponse response);
    
    /** 
     * Callback function that Halliday will execute after the user has logged out to clear sensitive member variables like private key and user information.
     */
    UFUNCTION()
        void _HandleLogout();
   
    /**
     *[PRIVATE MEMBER VARIABLES]
     */
    
    /** Store a single instance of Web3Auth in your application. Must have some Level or GameInstance object that contains this Blueprint Class. */
    AWeb3Auth* _Web3Auth;
    
    /** BlockchainType of your application. */
    EBlockchainType _BlockchainType;
    
    /** HTTP authorization request header that is used for all of your API calls. */
    FString _AuthHeaderValue;
    
    /** The base Halliday endpoint that your API calls will target depending on the production mode you provided in 'Initialize()'  */
    FString _ApiEndpoint;
    
    /** The private key of your player that will be used to sign onchain transactions. This is only set once the player logs in. */
    FString _PrivateKey;
    
    /** Id of the player that has logged in.  */
    FString _InGamePlayerId;
    
    /** UserInfo is a Web3Auth class that contains the details of your player's login. This is only set once the player logs in. */
    FUserInfo _UserInfo;
};
