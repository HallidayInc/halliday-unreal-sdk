#pragma once

#include "HallidayTypes.generated.h"

UENUM(BlueprintType)
enum class EBlockchainType : uint8
{
    ETHEREUM,
    GOERLI,
    POLYGON,
    MUMBAI,
    DFK,
    DFK_TESTNET,
    AVALANCHE_CCHAIN,
    ARBITRUM,
    ARBITRUM_GOERLI,
    OPTIMISM,
    OPTIMISM_GOERLI,
    BASE,
    BASE_GOERLI,
    KLAYTN_CYPRESS
};

UENUM(BlueprintType)
enum class EHallidayErrrorCode : uint8
{
    INVALID_PARAMETER,
    INTERNAL_ERROR,
    INCOMPLETE_CONFIG,
    USER_TOKEN_MISSING,
    SER_TOKEN_INVALID,
    USER_ALREADY_EXISTS,
    USER_DOES_NOT_EXIST,
    USER_NO_PHONE_NUMBER,
    USER_NO_EMAIL,
    USER_NO_NAME,
    USER_EMAIL_NOT_VERIFIED,
    AUTH_FAILED,
    AUTH_MISSING,
    USER_BANNED,
    USER_DEFAULTED,
    USER_UNDER_REVIEW,
    USER_NOT_SETUP_FOR_SUI_GAS_STATION,
    TXHASH_MISSING,
    TOKEN_TYPE_MISSING,
    AMOUNT_TOKEN_MISSING,
    NOT_SUPPORTED,
    LOCATION_NOT_SUPPORTED,
    OBJECT_DOES_NOT_EXIST,
    MULTIPLE_OBJECTS_FOR_SAME_ID,
    COLLECTION_NOT_ON_MARKETPLACE,
    COLLECTION_NOT_ALLOWED,
    ITEM_NOT_FOR_SALE,
    PRICE_OR_CURRENCY_NOT_SUPPORTED,
    NOT_A_BUSINESS_ACCOUNT,
    BLOCKCHAIN_TYPE_MISSING,
};

UENUM(BlueprintType)
enum class ETransactionType : uint8
{
    TRANSFER_ASSET,
    TRANSFER_BALANCE,
    CALL_CONTRACT,
};

// Client facing
USTRUCT(BlueprintType)
struct FWallet
{
    GENERATED_BODY()
    
    /** The name of the blockchain (e.g. ETHEREUM, POLYGON, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        EBlockchainType blockchain_type;

    /** Your in-game user ID for this user */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString in_game_player_id;

    /** The Halliday wallet address corresponding to this user */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString account_address;
};

USTRUCT(BlueprintType)
struct FGetWalletsResponse
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        TArray<FWallet> wallets;
};

USTRUCT(BlueprintType)
struct FAsset
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        EBlockchainType blockchain_type;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString collection_address;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString token_id;
};

USTRUCT(BlueprintType)
struct FGetAssetsResponse
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 num_assets;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        TArray<FAsset> assets;
};

USTRUCT(BlueprintType)
struct FERC20Token
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        EBlockchainType blockchain_type;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString token_address;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString balance;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int decimals;
};

USTRUCT(BlueprintType)
struct FNativeToken
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        EBlockchainType blockchain_type;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString balance;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 decimals;
};

USTRUCT(BlueprintType)
struct FGetBalancesResponse
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 num_erc20_tokens;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        TArray<FERC20Token> erc20_tokens;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 num_native_tokens;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        TArray<FNativeToken> native_tokens;
};

USTRUCT(BlueprintType)
struct FGetTransactionResponse
{
    GENERATED_BODY()
    
    /** The blockchain on which this transaction occurred (e.g. ETHEREUM, IMMUTABLE_PROD) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        EBlockchainType blockchain_type;
    
    /** The ID of the transaction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString tx_id;
    
    /** The status of the transaction (e.g. PENDING, COMPLETE, FAILED) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString status;
    
    /** The number of times we've retried sending this transaction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 retry_count;
    
    /** The blockchain transaction id if the transaction succeeded */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString on_chain_id;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString user_op_receipt;
    
    /** The error message if the transaction failed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString error_message;
};

USTRUCT(BlueprintType)
struct FBuildCalldataResponse
{
    GENERATED_BODY()
    
    /** Calldata that was built */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString calldata;
};

USTRUCT(BlueprintType)
struct FBigNumber
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString hex;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString type;
};

USTRUCT(BlueprintType)
struct FAATransaction
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString sender;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FBigNumber nonce;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString initCode;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString callData;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FBigNumber callGasLimit;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FBigNumber verificationGasLimit;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FBigNumber preVerificationGas;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FBigNumber maxFeePerGas;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FBigNumber maxPriorityFeePerGas;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString paymasterAndData;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        FString signature;
};

USTRUCT(BlueprintType)
struct FBuildTransactionResponse
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString tx_id;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FAATransaction transaction;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString tx_hash;
};

USTRUCT(BlueprintType)
struct FSubmitTransactionResponse
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString tx_id;
};

/** Internal use only. You should never need to interface with this response. */
USTRUCT(BlueprintType)
struct FGetSignerPublicAddressResponse
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString address;
};

/** Internal use only. You should never need to interface with this response. */
USTRUCT(BlueprintType)
struct FKeccak256Response
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString hashed_message;
};
