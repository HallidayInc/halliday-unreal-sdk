#include "Halliday.h"
#include "Kismet/GameplayStatics.h"
#include "secp256k1.h"
#include "secp256k1_recovery.h"
#include <assert.h>
#include <string.h>
#include <cstddef>
#include <cstdint>

// Include the following libraries depending on OS.
// These are used for FillRandom()
#if defined(_WIN32)
#include <Windows.h>
#include <bcrypt.h>
#elif defined(__linux__) || defined(__FreeBSD__)
#include <sys/random.h>
#elif defined(__APPLE__)
#include <Security/Security.h>
#endif

/**
 * Fill a byte array with random data. Used for secp256k1 signing.
 * @param Data Byte array
 * @param Size Size of the byte array
 * @returns A boolean to indicate success or failure.
 */
static bool FillRandom(uint8_t* Data, size_t Size) {
#if defined(_WIN32)
    NTSTATUS res = BCryptGenRandom(nullptr, Data, static_cast<ULONG>(Size), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return BCRYPT_SUCCESS(res) && Size <= ULONG_MAX;
#elif defined(__linux__) || defined(__FreeBSD__)
    sSize_t res = getrandom(Data, Size, 0);
    return res >= 0 && static_cast<Size_t>(res) == Size;
#elif defined(__APPLE__)
    return SecRandomCopyBytes(kSecRandomDefault, Size, Data) == errSecSuccess;
#endif
    return false;
}

/**
 * Convert a character into its hex value. Used for sec256k1 signing.
 * @param Char Character to convert
 * @returns The hex value corresponding to the input
 */
static int32 CharToHex(TCHAR Char) {
    if (Char >= '0' && Char <= '9') return Char - '0';
    if (Char >= 'a' && Char <= 'f') return 10 + Char - 'a';
    if (Char >= 'A' && Char <= 'F') return 10 + Char - 'A';
    return -1; // or handle invalid character
}

/**
 * Convert any response message body into the template object.
 * This is used in all the _Handle methods.
 */
template<typename TResponseType>
static TResponseType ParseResponse(const FString& MessageBody)
{
    TResponseType ResponseObject;
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MessageBody);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), TResponseType::StaticStruct(), &ResponseObject, 0, 0);
    }

    return ResponseObject;
}

/**
 * Convert an object to a string for logging.
 * @param Object Object to convert.
 */
template<typename TObjectType>
static FString ObjectToString(const TObjectType& Object)
{
    FString OutputString;
    if (FJsonObjectConverter::UStructToJsonObjectString(TObjectType::StaticStruct(), &Object, OutputString, 0, 0))
    {
        return OutputString;
    }

    return TEXT("");
}

/**
 * Convert a BlockchainType enum value to its equivalent string value that in API calls.
 * @param BlockchainType Enum value of EBlockchainType.
 * @returns A string representation the blockchain you want to use.
 */
static FString BlockchainTypeToString(const EBlockchainType& BlockchainType) {
    switch(BlockchainType) {
        case EBlockchainType::ETHEREUM:
            return TEXT("ethereum");
        case EBlockchainType::GOERLI:
            return TEXT("goerli");
        case EBlockchainType::POLYGON:
            return TEXT("polygon");
        case EBlockchainType::MUMBAI:
            return TEXT("mumbai");
        case EBlockchainType::DFK:
            return TEXT("dfk");
        case EBlockchainType::DFK_TESTNET:
            return TEXT("dfk_testnet");
        case EBlockchainType::AVALANCHE_CCHAIN:
            return TEXT("avalanche_cchain");
        case EBlockchainType::ARBITRUM:
            return TEXT("arbitrum");
        case EBlockchainType::ARBITRUM_GOERLI:
            return TEXT("arbitrum_goerli");
        case EBlockchainType::OPTIMISM:
            return TEXT("optimism");
        case EBlockchainType::OPTIMISM_GOERLI:
            return TEXT("optimism_goerli");
        case EBlockchainType::BASE:
            return TEXT("base");
        case EBlockchainType::BASE_GOERLI:
            return TEXT("base_goerli");
        case EBlockchainType::KLAYTN_CYPRESS:
            return TEXT("klaytn_cypress");
        default:
            UE_LOG(LogTemp, Error, TEXT("invalid blockchain"));
            return TEXT("INVALID BLOCKCHAIN");
    }
}

/**
 * Convert a TransactionType enum value to its equivalent string value that is used to form a URL.
 * @param TxType Enum value of ETransactionType
 * @returns A string representation which endpoint path to hit.
 */
static FString TransactionTypeToString(const ETransactionType& TxType) {
    switch(TxType) {
        case ETransactionType::TRANSFER_ASSET:
            return TEXT("transferAsset");
        case ETransactionType::TRANSFER_BALANCE:
            return TEXT("transferBalance");
        case ETransactionType::CALL_CONTRACT:
            return TEXT("contract");
        default:
            UE_LOG(LogTemp, Error, TEXT("Invalid tx type."));
            return TEXT("INVALID TX TYPE");
    }
}

/**
 * Member functions
 */
AHalliday::AHalliday() {
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
   PrimaryActorTick.bCanEverTick = true;
}

AWeb3Auth* AHalliday::GetWeb3Auth()
{
    return _Web3Auth;
}

EBlockchainType AHalliday::GetBlockchainType()
{
    return _BlockchainType;
}

FString AHalliday::GetAuthHeaderValue()
{
    return _AuthHeaderValue;
}

FString AHalliday::GetApiEndpoint()
{
    return _ApiEndpoint;
}

FString AHalliday::GetInGamePlayerId()
{
    return _InGamePlayerId;
}

FUserInfo AHalliday::GetUserInfo()
{
    return _UserInfo;
}

void AHalliday::SetInGamePlayerId(const FString& InGamePlayerId)
{
    _InGamePlayerId = InGamePlayerId;
}

FString AHalliday::_GetPublicKeyFromPrivateKey()
{
    // Convert the hex string to a byte array.
    unsigned char SecretKey[32];
    for (int32 i = 0; i < 32; ++i) {
        // Convert each pair of hex characters to a byte
        // Nibbles are a half byte. Low nibble is bits 0 through 3 and high nibble is bits 4 through 7.
        TCHAR HighNibble = _PrivateKey[i * 2];
        TCHAR LowNibble = _PrivateKey[i * 2 + 1];
        SecretKey[i] = (CharToHex(HighNibble) << 4) + CharToHex(LowNibble);
    }
    
    secp256k1_pubkey PublicKey;
    
    // Create a context for the secp256k1 library using 'SECP256K1_CONTEXT_NONE'.
    // All other contexts in the library have been deprecated. This will allow for all functionality.
    secp256k1_context* Ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
   
    /* Randomizing the context is recommended to protect against side-channel
     * leakage See `secp256k1_context_randomize` in secp256k1.h for more
     * information about it. This should never fail. */
    unsigned char Randomize[32];
    if (!FillRandom(Randomize, sizeof(Randomize))) {
        UE_LOG(LogTemp, Error, TEXT("Unexpected response when getting the public address of player '%s'."), *(_InGamePlayerId));
        return "";
    }
    if(secp256k1_context_randomize(Ctx, Randomize) != 1) {
        UE_LOG(LogTemp, Error, TEXT("Failed to randomize the context for player '%s'."), *(_InGamePlayerId));
        return "";
    }
    
    // Get the public key.
    if(secp256k1_ec_pubkey_create(Ctx, &PublicKey, SecretKey) != 1) {
        UE_LOG(LogTemp, Error, TEXT("Failed to generate the public key for player '%s'."), *(_InGamePlayerId));
        return "";
    }
    
    // Serialize the public key.
    unsigned char PublicKeySerialized[65] = {0};
    size_t Len = sizeof(PublicKeySerialized);
    secp256k1_ec_pubkey_serialize(Ctx, PublicKeySerialized, &Len, &PublicKey, SECP256K1_EC_UNCOMPRESSED);

    // Convert the byte array into a hex string. This will NOT have "0x" as a prefix.
    FString PublicKeyHexString;
    for (size_t i = 0; i < Len; ++i) {
        PublicKeyHexString += FString::Printf(TEXT("%02x"), PublicKeySerialized[i]);
    }
    
    // Clean up the context.
    secp256k1_context_destroy(Ctx);
    
    return PublicKeyHexString;
}

FString AHalliday::_Secp256k1(FString TxHash) {
    unsigned char SecKey[32];
    for (int32 i = 0; i < 32; ++i) {
        // Convert each pair of hex characters to a byte
        // Nibbles are a half byte. Low nibble is bits 0 through 3 and high nibble is bits 4 through 7.
        TCHAR HighNibble = _PrivateKey[i * 2];
        TCHAR LowNibble = _PrivateKey[i * 2 + 1];
        SecKey[i] = (CharToHex(HighNibble) << 4) + CharToHex(LowNibble);
    }
    
    // Create a secp256k1 context variable. This will be default enable us to sign and verify.
    secp256k1_context* Ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    secp256k1_ecdsa_recoverable_signature Signature;
    
    // Convert the transaction hash into a byte array.
    unsigned char MsgHash[32];
    for(int32 i = 0; i < 32; ++i)
    {
        TCHAR HighNibble = TxHash[2 + i * 2];
        TCHAR LowNibble = TxHash[2 + i * 2 + 1];
        MsgHash[i] = (CharToHex(HighNibble) << 4) + CharToHex(LowNibble);
    }
    
    // Get the recoverable signature.
    secp256k1_ecdsa_sign_recoverable(Ctx, &Signature, MsgHash, SecKey, NULL, NULL);
    
    // Serialize the recoverable signature into a compact signature.
    unsigned char SerializedSignature[65] = {0};
    int RecoveryId = 0;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(Ctx, SerializedSignature, &RecoveryId, &Signature);
    
    // Set the v component of the signature using the recovery id given by the serialize function.
    SerializedSignature[64] = (uint8)(27 + RecoveryId);
    
    // Convert the byte array to a hex string.
    FString SignatureAsHexString = "0x";
    for (int i = 0; i < 65; ++i) {
        SignatureAsHexString += FString::Printf(TEXT("%02x"), SerializedSignature[i]);
    }
    
    // Clean up the context varaible.
    secp256k1_context_destroy(Ctx);
    
    return SignatureAsHexString;
}

void AHalliday::Initialize(const FString& PublicApiKey, EBlockchainType BlockchainType, bool bIsSandbox, const FString& ClientVerifierId) {
    _AuthHeaderValue = FString(TEXT("Bearer ")) + PublicApiKey;
    _BlockchainType = BlockchainType;

    // Find an instance of Web3 Auth within the application.
    AWeb3Auth* FoundWeb3AuthInstance = Cast<AWeb3Auth>(UGameplayStatics::GetActorOfClass(GetWorld(), AWeb3Auth::StaticClass()));
    if (FoundWeb3AuthInstance)
    {
        _Web3Auth = FoundWeb3AuthInstance;
    } else {
        UE_LOG(LogTemp, Error, TEXT("Failed to initialize because an instance of Web3Auth was not found."));
        return;
    }
    
    // Set options for web3Auth (assuming Web3Auth is an object of a class that you have access to)
    FWeb3AuthOptions Web3AuthOptions;
    Web3AuthOptions.clientId = ClientVerifierId;
    Web3AuthOptions.network = FNetwork::CYAN; // Adjust according to your actual enum
    
    _Web3Auth->setOptions(Web3AuthOptions);

    // Set API endpoint durin
    if (bIsSandbox)
    {
        _ApiEndpoint = TEXT("https://sandbox.halliday.xyz/v1/");
    }
    else
    {
        _ApiEndpoint = TEXT("https://api.halliday.xyz/v1/");
    }
}

/**
 * This function is called by the social log in functions like LogInWithGoogle().
 * @param Web3Auth Instance of Web3Auth that is found in game.
 * @param Provider The login provider to log in with.
 * @param EmailAddress Optional parameter if logging in with email OTP.
 */
void _LogInHelper(AWeb3Auth* Web3Auth, FProvider Provider, const FString& EmailAddress = "") {
    // Set the login parameter options to include the login provider
    FLoginParams LoginParams;
    
    switch(Provider) {
        case FProvider::GOOGLE:
            LoginParams.loginProvider = TEXT("google");
            break;
        case FProvider::FACEBOOK:
            LoginParams.loginProvider = TEXT("facebook");
            break;
        case FProvider::TWITTER:
            LoginParams.loginProvider = TEXT("twitter");
            break;
        case FProvider::EMAIL_PASSWORDLESS:
            LoginParams.loginProvider = TEXT("email_passwordless");
            break;
        default:
            UE_LOG(LogTemp, Error, TEXT("Invalid provider"));
            return;
    }

    // If the login provider is OTP then set the login hint as the email address
    if (Provider == FProvider::EMAIL_PASSWORDLESS) {
        LoginParams.extraLoginOptions.login_hint = EmailAddress;
    }

    // Call Web3 Auth to process the login
    if (Web3Auth)
    {
        Web3Auth->processLogin(LoginParams);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[Halliday Error] Cannot log in because _Web3Auth is not initialized."));
    }
}

void AHalliday::LogInWithGoogle() {
    _LogInHelper(_Web3Auth, FProvider::GOOGLE);
}

void AHalliday::LogInWithFacebook() {
    _LogInHelper(_Web3Auth, FProvider::FACEBOOK);
}

void AHalliday::LogInWithEmailOtp(const FString& EmailAddress) {
    _LogInHelper(_Web3Auth, FProvider::EMAIL_PASSWORDLESS, EmailAddress);
}

void AHalliday::SetLoginCompletedCallback(FOnLoginCompleted Callback) {
    OnLoginCompleted = Callback;
}

void AHalliday::SetLogoutCompletedCallback(FOnLogoutCompleted Callback) {
    OnLogoutCompleted = Callback;
}

void AHalliday::_HandleLogin(FWeb3AuthResponse response) {
    // Store the user informaton that Web3 Auth returns.
    _PrivateKey = response.privKey;
    _UserInfo = response.userInfo;
    
    // Execute the callback that was previously passed in.
    // This uses the main game thread so this must complete before moving forward.
    AsyncTask(ENamedThreads::GameThread, [this]() {
        OnLoginCompleted.ExecuteIfBound();
    });
}

void AHalliday::_HandleLogout() {
    // Clear important user information.
    _PrivateKey = TEXT(""); // IMPORTANT! DO NOT REMOVE!
    _UserInfo.email = TEXT("");
    _UserInfo.name = TEXT("");
    _UserInfo.profileImage = TEXT("");
    _UserInfo.aggregateVerifier = TEXT("");
    _UserInfo.verifier = TEXT("");
    _UserInfo.verifierId = TEXT("");
    _UserInfo.typeOfLogin = TEXT("");
    _UserInfo.dappShare = TEXT("");
    _UserInfo.idToken = TEXT("");
    _UserInfo.oAuthIdToken = TEXT("");
    _UserInfo.oAuthAccessToken = TEXT("");
    
    // Execute the callback event that was previous set.
    AsyncTask(ENamedThreads::GameThread, [this]() {
        OnLogoutCompleted.ExecuteIfBound();
    });
}

/**
 * This function will call GetOrCreateHallidayAAWallet() again and you will receive a response through the FOnWalletReceived delegate.
 * INTERNAL FLOW:
 * 1. GetOrCreateHallidayAAWalletResponse
 * 2. _HandleGetOrCreateHallidayAAWalletResponse
 * 3. _GetSignerPublicAddress
 * 4. _HandleGetSignerPublicAddressResponse
 * 5. _CreateWallet
 * 6. _HandleCreateWalletResponse [Current Step]
 * 7. GetOrCreateHallidayAAWalletResponse
 *
 * @param Request Request sent from _CreateWallet().
 * @param Response Response from the request sent from _CreateWallet().
 * @param bWasSuccessful Indicates the success of the request.
 * @param Halliday Pointer to the object that called GetOrCreateHallidayAAWallet().
 * @param InGamePlayerId Id of the player to create a wallet for.
 */
void _HandleCreateWalletResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, AHalliday* Halliday, const FString& InGamePlayerId)
{
    if (bWasSuccessful && Response->GetResponseCode() == 200)
    {
        // Now that the wallet was succssfuly created, call GetOrCreateHallidayAAWallet again.
        // This time pass in bWasPreviouslyCalled = true in order to prevent an infinite loop.
        Halliday->GetOrCreateHallidayAAWallet(InGamePlayerId, true);
    }
    else
    {
        FString ResponseError = Response->GetContentAsString();
        UE_LOG(LogTemp, Error, TEXT("[Halliday Error] Failed to create a wallet for player '%s' because '%s'."), *InGamePlayerId, *ResponseError);
    }
}

/**
 * Create a new wallet if GetOrCreateHallidayAAWalletResponse() returns 0 wallets or a list of wallets that does not contain a wallet with your desired blockchain.
 * * INTERNAL FLOW:
 * 1. GetOrCreateHallidayAAWalletResponse
 * 2. _HandleGetOrCreateHallidayAAWalletResponse
 * 3. _GetSignerPublicAddress
 * 4. _HandleGetSignerPublicAddressResponse
 * 5. _CreateWallet [Current Step]
 * 6. _HandleCreateWalletResponse
 * 7. GetOrCreateHallidayAAWalletResponse
 *
 * @param Halliday Pointer to the object that called GetOrCreateHallidayAAWallet().
 * @param InGamePlayerId Id of the player to create a wallet for.
 * @param SignerPublicAddress Public address of the private key from Web3Auth formatted as a hexstring WITHOUT "0x" as the prefix.
 */
void _CreateWallet(AHalliday* Halliday, const FString& InGamePlayerId, const FString& SignerPublicAddress)
{
    FString NewAccountUrl = Halliday->GetApiEndpoint() + TEXT("client/accounts");

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(NewAccountUrl);
    Request->SetVerb("POST");
    Request->SetHeader(TEXT("Authorization"), Halliday->GetAuthHeaderValue());
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    
    // Bind a callback function to handle the response.
    Request->OnProcessRequestComplete().BindLambda([Halliday, InGamePlayerId](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
       _HandleCreateWalletResponse(Request, Response, bWasSuccessful, Halliday, InGamePlayerId);
    });

    // Create the request body as JSON
    TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
    RequestBody->SetStringField(TEXT("email"), Halliday->GetUserInfo().email);
    RequestBody->SetStringField(TEXT("in_game_player_id"), InGamePlayerId);
    RequestBody->SetStringField(TEXT("non_custodial_address"), SignerPublicAddress);
    RequestBody->SetStringField(TEXT("blockchain_type"), BlockchainTypeToString(Halliday->GetBlockchainType()));

    FString RequestBodyAsString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyAsString);
    FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

    Request->SetContentAsString(RequestBodyAsString);
    Request->ProcessRequest();
}

/**
 * This function will parse the non-custodial wallet address aka owner's public address and pass it to _CreateWallet().
 * INTERNAL FLOW:
 * 1. GetOrCreateHallidayAAWalletResponse
 * 2. _HandleGetOrCreateHallidayAAWalletResponse
 * 3. _GetSignerPublicAddress
 * 4. _HandleGetSignerPublicAddressResponse [Current Step]
 * 5. _CreateWallet
 * 6. _HandleCreateWalletResponse
 * 7. GetOrCreateHallidayAAWalletResponse
 *
 * @param Request Request sent from _GetSignerPublicAddress().
 * @param Response Response from the request sent from _GetSignerPublicAddress().
 * @param bWasSuccessful Indicates the success of the request.
 * @param Halliday Pointer to the object that called GetOrCreateHallidayAAWallet().
 * @param InGamePlayerId Id of the player to create a wallet for.
 */
void _HandleGetSignerPublicAddressResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, AHalliday* Halliday, const FString& InGamePlayerId)
{
    if (bWasSuccessful && Response->GetResponseCode() == 200)
    {
        FString MessageBody = Response->GetContentAsString();
        FGetSignerPublicAddressResponse GetSignerPublicAddressResponse = ParseResponse<FGetSignerPublicAddressResponse>(MessageBody);

        // Create a new wallet after obtaining the address of the public key.
        // The wallet address created with this function will NOT by the address of the public key.
        _CreateWallet(Halliday, InGamePlayerId, GetSignerPublicAddressResponse.address);
    }
    else
    {
        // Account owner means the owner or non-custodial public address. NOT the address where the user stores their assets.
        UE_LOG(LogTemp, Error, TEXT("[Halliday Error] Failed to get the public wallet address of the account owner."));
    }
}

/**
 * Calls the Halliday backend to get the public address from the public key. This function assumes that the private key in Halliday* has been set and that the user is logged in.
 * This call is necessary for _CreateWallet() to work.
 * * INTERNAL FLOW:
 * 1. GetOrCreateHallidayAAWalletResponse
 * 2. _HandleGetOrCreateHallidayAAWalletResponse
 * 3. _GetSignerPublicAddress [Current Step]
 * 4. _HandleGetSignerPublicAddressResponse
 * 5. _CreateWallet
 * 6. _HandleCreateWalletResponse
 * 7. GetOrCreateHallidayAAWalletResponse
 *
 * @param Halliday Pointer to the object that called GetOrCreateHallidayAAWallet().
 * @param InGamePlayerId Id of the player to create a wallet for.
 */
void _GetSignerPublicAddress(AHalliday* Halliday, const FString& InGamePlayerId)
{
    FString PublicKey = Halliday->_GetPublicKeyFromPrivateKey();
    
    // Call Halliday backend to retrieve the public address corresponding to this key.
    FString GetPublicAddressUrl = Halliday->GetApiEndpoint() + TEXT("client/getAddressFromPublicKey?public_key=") + PublicKey;
    
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(GetPublicAddressUrl);
    Request->SetVerb("GET");
    Request->SetHeader("Authorization", Halliday->GetAuthHeaderValue());
    
    // Bind a callback function to handle the response.
    Request->OnProcessRequestComplete().BindLambda([Halliday, InGamePlayerId](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
       _HandleGetSignerPublicAddressResponse(Request, Response, bWasSuccessful, Halliday, InGamePlayerId);
    });
    
    Request->ProcessRequest();
}

/**
 * Handle the response of GetOrCreateHallidayAAWallet(). Broadcasts a wallet if found, otherwise it will start the internal flow to generate a new wallet.
 * INTERNAL FLOW:
 * 1. GetOrCreateHallidayAAWalletResponse
 * 2. _HandleGetOrCreateHallidayAAWalletResponse [Current Step]
 * 3. _GetSignerPublicAddress
 * 4. _HandleGetSignerPublicAddressResponse
 * 5. _CreateWallet
 * 6. _HandleCreateWalletResponse
 * 7. GetOrCreateHallidayAAWalletResponse
 *
 * @param Request Request sent from _GetSignerPublicAddress().
 * @param Response Response from the request sent from _GetSignerPublicAddress().
 * @param bWasSuccessful Indicates the success of the request.
 * @param Halliday Pointer to the object that called GetOrCreateHallidayAAWallet().
 * @param InGamePlayerId Id of the player to create a wallet for.
 * @param bWasPreviouslyCalled Indicates whether we should try to create a wallet again if not found.
 */
void _HandleGetOrCreateHallidayAAWalletResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, AHalliday* Halliday, const FString& InGamePlayerId, bool bWasPreviouslyCalled) {
    if (bWasSuccessful && Response->GetResponseCode() == 200)
    {
        FString MessageBody = Response->GetContentAsString();
        FGetWalletsResponse GetWalletsResponse = ParseResponse<FGetWalletsResponse>(MessageBody);
        
        bool bIsWalletFound = false;
        for (const FWallet& Wallet : GetWalletsResponse.wallets)
        {
            if (Wallet.blockchain_type == Halliday->GetBlockchainType())
            {
                UE_LOG(LogTemp, Display, TEXT("[Halliday Response] Fetched wallet for player '%s': %s"), *InGamePlayerId, *(ObjectToString<FWallet>(Wallet)));
                
                // Broadcast the wallet data.
                Halliday->OnWalletReceived.Broadcast(Wallet);
                bIsWalletFound = true;
                break; // Break the loop if the desired wallet is found.
            }
        }
        
        // If there was no wallet on the blockchain you desired, we will create one.
        // The first step in wallet creation is to get the non-custodial wallet address.
        if(!bIsWalletFound) {
            _GetSignerPublicAddress(Halliday, InGamePlayerId);
        }
    } else
    {
        FString ResponseError = Response->GetContentAsString();
        if(!bWasPreviouslyCalled)
        {
            TSharedPtr<FJsonObject> JsonObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseError);

            if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
            {
                // Check the user has been created on the server yet.
                if (JsonObject->HasField("code") && JsonObject->GetStringField("code") == "USER_DOES_NOT_EXIST")
                {
                    // Create a wallet if there is no account associated with this player id.
                    // The first step in wallet creation is to get the non-custodial wallet address.
                    _GetSignerPublicAddress(Halliday, InGamePlayerId);
                }
            }
        } else {
            UE_LOG(LogTemp, Error, TEXT("[Halliday Error] Failed to get or create a wallet for player '%s' because '%s'."), *InGamePlayerId, *ResponseError);
        }
    }
}

void AHalliday::GetOrCreateHallidayAAWallet(const FString& InGamePlayerId, bool bWasPreviouslyCalled) {
    // Save the InGamePlayerId
    _InGamePlayerId = InGamePlayerId;
    
    FString GetPlayerWalletsUrl = _ApiEndpoint + TEXT("client/accounts/") + InGamePlayerId + TEXT("/wallets");
    
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(GetPlayerWalletsUrl);
    Request->SetVerb("GET");
    Request->SetHeader("Authorization", _AuthHeaderValue);
    
    // Bind a callback function to handle the response from the Halliday backend server.
    Request->OnProcessRequestComplete().BindLambda([this, InGamePlayerId, bWasPreviouslyCalled](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
       _HandleGetOrCreateHallidayAAWalletResponse(Request, Response, bWasSuccessful, this, InGamePlayerId, bWasPreviouslyCalled);
    });

    Request->ProcessRequest();
}

/**
 * Asynchronous callback function to trigger a delegate broadcast once the HTTP request in GetAssets() is complete.
 * @param Request Request sent from GetAssets().
 * @param Response Response from the request sent from GetAssets().
 * @param bWasSuccessful Indicates the success of the request.
 * @param Halliday Pointer to the object that called GetAssets().
 * @param InGamePlayerId Id of the player who owns the assets.
 */
void _HandleGetAssetsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, AHalliday* Halliday, const FString& InGamePlayerId)
{
    if (bWasSuccessful && Response->GetResponseCode() == 200)
    {
        // Convert the response body.
        FString MessageBody = Response->GetContentAsString();
        FGetAssetsResponse GetAssetsResponse = ParseResponse<FGetAssetsResponse>(MessageBody);
        
        // Broadcast the wallet data.
        Halliday->OnAssetsReceived.Broadcast(GetAssetsResponse);
        
        UE_LOG(LogTemp, Display, TEXT("[Halliday Response] Fetched assets for player '%s': %s"), *InGamePlayerId, *(ObjectToString(GetAssetsResponse)));
    }
    else
    {
        FString ResponseError = Response->GetContentAsString();
        UE_LOG(LogTemp, Error, TEXT("[Halliday Error] Failed to call GetAssets() for player '%s' because '%s'."), *InGamePlayerId, *ResponseError);
    }
}

void AHalliday::GetAssets(const FString& InGamePlayerId)
{
    FString GetPlayerAssetsUrl = _ApiEndpoint + TEXT("client/accounts/") + InGamePlayerId + TEXT("/assets");
    
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(GetPlayerAssetsUrl);
    Request->SetVerb("GET");
    Request->SetHeader("Authorization", _AuthHeaderValue);
    
    // Bind a callback to process the HTTP response
    Request->OnProcessRequestComplete().BindLambda([this, InGamePlayerId](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
       _HandleGetAssetsResponse(Request, Response, bWasSuccessful, this, InGamePlayerId);
    });
    
    Request->ProcessRequest();
}

/**
 * Asynchronous callback function to trigger a delegate broadcast once the HTTP request in GetBalances() is complete.
 * @param Request Request sent from GetAssets().
 * @param Response Response from the request sent from GetAssets().
 * @param bWasSuccessful Indicates the success of the request.
 * @param Halliday Pointer to the object that called GetAssets().
 * @param InGamePlayerId Id of the player who owns the assets.
 */
void _HandleGetBalancesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, AHalliday* Halliday, const FString& InGamePlayerId)
{
    if (bWasSuccessful && Response->GetResponseCode() == 200)
    {
        // Convert the response body.
        FString MessageBody = Response->GetContentAsString();
        FGetBalancesResponse GetBalancesResponse = ParseResponse<FGetBalancesResponse>(MessageBody);
        
        // Broadcast the wallet data. The client should bind a callback to the delegate to receive this response.
        Halliday->OnBalancesReceived.Broadcast(GetBalancesResponse);
        
        UE_LOG(LogTemp, Display, TEXT("[Halliday Response] Fetched balances for player '%s': %s"), *InGamePlayerId, *(ObjectToString(GetBalancesResponse)));
    }
    else
    {
        FString ResponseError = Response->GetContentAsString();
        UE_LOG(LogTemp, Error, TEXT("[Halliday Error] Failed to call GetBalances() for player '%s' because '%s'."), *InGamePlayerId, *ResponseError);
    }
}

void AHalliday::GetBalances(const FString& InGamePlayerId)
{
    FString GetPlayerBalancesUrl = _ApiEndpoint + TEXT("client/accounts/") + InGamePlayerId + TEXT("/balances");
    
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(GetPlayerBalancesUrl);
    Request->SetVerb("GET");
    Request->SetHeader("Authorization", _AuthHeaderValue);
    
    // Bind a callback to process the HTTP response
    Request->OnProcessRequestComplete().BindLambda([this, InGamePlayerId](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
       _HandleGetBalancesResponse(Request, Response, bWasSuccessful, this, InGamePlayerId);
    });
    
    Request->ProcessRequest();
}

/**
 * Asynchronous callback function to trigger a delegate broadcast once the HTTP request in GetTransaction() is complete.
 * @param Request Request sent from GetTransaction().
 * @param Response Response from the request sent from GetTransaction().
 * @param bWasSuccessful Indicates the success of the request.
 * @param Halliday Pointer to the object that called GetTransaction().
 * @param TxId Id of the transaction to fetch.
 */
void _HandleGetTransactionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, AHalliday* Halliday, const FString& TxId)
{
    if (bWasSuccessful && Response->GetResponseCode() == 200)
    {
        // Convert the response body to a string and then use the helper function to convert it into the expected response object.
        FString MessageBody = Response->GetContentAsString();
        FGetTransactionResponse GetTransactionResponse = ParseResponse<FGetTransactionResponse>(MessageBody);
        
        // Broadcast the wallet data
        Halliday->OnTransactionReceived.Broadcast(GetTransactionResponse);
        
        UE_LOG(LogTemp, Display, TEXT("[Halliday Response] Fetched transaction: %s"), *(ObjectToString(GetTransactionResponse)));
    }
    else
    {
        FString ResponseError = Response->GetContentAsString();
        UE_LOG(LogTemp, Error, TEXT("[Halliday Error] Failed to call GetTransaction() for transaction id '%s' because '%s'."), *TxId, *ResponseError);
    }
}

void AHalliday::GetTransaction(const FString& TxId)
{
    FString GetTransactionUrl = _ApiEndpoint + TEXT("client/transactions/") + TxId;
    
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(GetTransactionUrl);
    Request->SetVerb("GET");
    Request->SetHeader("Authorization", _AuthHeaderValue);
    
    // Bind a callback to process the HTTP response
    Request->OnProcessRequestComplete().BindLambda([this, TxId](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
       _HandleGetTransactionResponse(Request, Response, bWasSuccessful, this, TxId);
    });
    
    Request->ProcessRequest();
}

/**
 * Asynchronous callback function to trigger a delegate broadcast once the transaction is submitted.
 * INTERNAL FLOW:
 * 1.  TransferAsset(), TransferBalance(), or ContractCall()
 * 2. _BuildTransaction()
 * 3. _HandleBuildTransactionResponse()
 * 4. _Keccak256()
 * 5. _HandleKaccek256Response()
 * 6. _SignAndSubmitTransaction()
 * 7. _HandleSignAndSubmitTransaction() [Current Step]
 *
 * @param Request Request sent from _Keccak256().
 * @param Response Response from the request sent from _Keccak256().
 * @param bWasSuccessful Indicates the success of the request.
 * @param Halliday Pointer to the object that called TransferAsset(), TransferBalance(), or ContractCall().
 * @param FromInGamePlayerId Id of the player to create a wallet for.
 * @param TxType Type of transaction that is being called.
 */
void _HandleSignAndSubmitTransactionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, AHalliday* Halliday, const FString& FromInGamePlayerId, ETransactionType TxType)
{
    if (bWasSuccessful && Response->GetResponseCode() == 202)
    {
        FString MessageBody = Response->GetContentAsString();
        FSubmitTransactionResponse SubmitTransactionResponse = ParseResponse<FSubmitTransactionResponse>(MessageBody);
        
        switch(TxType) {
            case ETransactionType::TRANSFER_ASSET:
                UE_LOG(LogTemp, Display, TEXT("[Halliday Response] Submitted a transaction '%s' to transfer an asset for player '%s': %s"), *SubmitTransactionResponse.tx_id, *FromInGamePlayerId, *(ObjectToString(SubmitTransactionResponse)));
                Halliday->OnTransferAssetSubmitted.Broadcast(SubmitTransactionResponse);
                break;
            case ETransactionType::TRANSFER_BALANCE:
                UE_LOG(LogTemp, Display, TEXT("[Halliday Response] Submitted a transaction '%s' to transfer a balance for player '%s': %s"), *SubmitTransactionResponse.tx_id, *FromInGamePlayerId, *(ObjectToString(SubmitTransactionResponse)));
                Halliday->OnTransferBalanceSubmitted.Broadcast(SubmitTransactionResponse);
                break;
            case ETransactionType::CALL_CONTRACT:
                UE_LOG(LogTemp, Display, TEXT("[Halliday Response] Submitted a transaction '%s' to call a contract for player '%s': %s"), *SubmitTransactionResponse.tx_id, *FromInGamePlayerId, *(ObjectToString(SubmitTransactionResponse)));
                Halliday->OnCallContractSubmitted.Broadcast(SubmitTransactionResponse);
                break;
            default:
                // Should never happen.
                UE_LOG(LogTemp, Error, TEXT("[Halliday Error] Invalid TxType in when signing and submitting a transaction."));
                break;
        }
        
        // TODO add retry logic in case of failures.
    }
    else
    {
        FString ResponseError = Response->GetContentAsString();
        UE_LOG(LogTemp, Error, TEXT("[Halliday Error] Failed to sign and submit a transaction for player '%s' because '%s'."), *FromInGamePlayerId, *(ResponseError));
    }
}

/**
 * Signs a Keccak256 hashed TxHash and submit a transaction to the Halliday backend for onchain execution.
 * INTERNAL FLOW:
 * 1.  TransferAsset(), TransferBalance(), or ContractCall()
 * 2. _BuildTransaction()
 * 3. _HandleBuildTransactionResponse()
 * 4. _Keccak256()
 * 5. _HandleKaccek256Response()
 * 6. _SignAndSubmitTransaction() [Current Step]
 * 7. _HandleSignAndSubmitTransaction()
 *
 * @param Halliday Pointer to the object that called Pointer to the object that called TransferAsset(), TransferBalance(), or ContractCall().
 * @param BuildTransactionResponse Response object from _BuildTransaction().
 * @param FromInGamePlayerId Player to build a transaction for.
 * @param Keccak256HashedTransactionHash Hashed tx hash that is going to be signed.
 * @param TxType Type of transaction that is being called.
 */
void _SignAndSubmitTransaction(AHalliday* Halliday, const FBuildTransactionResponse& BuildTransactionResponse, const FString& FromInGamePlayerId, const FString& Keccak256HashedTransactionHash, ETransactionType TxType)
{
    FAATransaction Transaction = BuildTransactionResponse.transaction;
    FString SignatureString = Halliday->_Secp256k1(Keccak256HashedTransactionHash);
    Transaction.signature = SignatureString;

    FString SubmitUrl = Halliday->GetApiEndpoint() + TEXT("client/transactions/");
    
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(SubmitUrl);
    Request->SetVerb("POST");
    Request->SetHeader(TEXT("Authorization"), Halliday->GetAuthHeaderValue());
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    
    // Bind a callback to handle the response.
    Request->OnProcessRequestComplete().BindLambda([Halliday, FromInGamePlayerId, TxType](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
       _HandleSignAndSubmitTransactionResponse(Request, Response, bWasSuccessful, Halliday, FromInGamePlayerId, TxType);
    });
    
    // Convert the transaction object into a general JSON object.
    TSharedPtr<FJsonObject> TransactionJsonObject = MakeShared<FJsonObject>();
    FJsonObjectConverter::UStructToJsonObject(FAATransaction::StaticStruct(), &Transaction, TransactionJsonObject.ToSharedRef(), 0, 0);
    
    // Create a the request body.
    TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
    RequestBody->SetStringField(TEXT("from_in_game_player_id"), FromInGamePlayerId);
    RequestBody->SetObjectField(TEXT("signed_tx"), TransactionJsonObject);
    RequestBody->SetStringField(TEXT("blockchain_type"), BlockchainTypeToString(Halliday->GetBlockchainType()));
    RequestBody->SetStringField(TEXT("tx_id"), BuildTransactionResponse.tx_id);

    // Convert the request body from JSON to string.
    FString RequestBodyAsString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyAsString);
    FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);
    
    Request->SetContentAsString(RequestBodyAsString);
    Request->ProcessRequest();
}

/**
 *
 * Asynchronous callback function to send the hashed message to _SignAndSubmitTransaction().
 * INTERNAL FLOW:
 * 1.  TransferAsset(), TransferBalance(), or ContractCall()
 * 2. _BuildTransaction()
 * 3. _HandleBuildTransactionResponse()
 * 4. _Keccak256()
 * 5. _HandleKaccek256Response() [Current Step]
 * 6. _SignAndSubmitTransaction()
 * 7. _HandleSignAndSubmitTransaction()
 *
 * @param Request Request sent from _Keccak256().
 * @param Response Response from the request sent from _Keccak256().
 * @param bWasSuccessful Indicates the success of the request.
 * @param Halliday Pointer to the object that called TransferAsset(), TransferBalance(), or ContractCall().
 * @param FromInGamePlayerId Id of the player to create a wallet for.
 * @param TxType Type of transaction that is being called.
 */
void _HandleKeccak256Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, const FBuildTransactionResponse& BuildTransactionResponse, AHalliday* Halliday, const FString& FromInGamePlayerId, ETransactionType TxType)
{
    if (bWasSuccessful && Response->GetResponseCode() == 200)
    {
        FString MessageBody = Response->GetContentAsString();
        FKeccak256Response Keccak256Response = ParseResponse<FKeccak256Response>(MessageBody);
    
        _SignAndSubmitTransaction(Halliday, BuildTransactionResponse, FromInGamePlayerId, Keccak256Response.hashed_message, TxType);
    }
    else
    {
        FString ResponseError = Response->GetContentAsString();
        UE_LOG(LogTemp, Error, TEXT("[Halliday Error] Failed to hash the transaction for player '%s' because '%s'."), *FromInGamePlayerId, *ResponseError);
    }
}

/**
 * Helper function that calls our backend to hash the transaction hash using Keccak256. This is to avoid using an unreliable library in C++.
 * INTERNAL FLOW:
 * 1.  TransferAsset(), TransferBalance(), or ContractCall()
 * 2. _BuildTransaction()
 * 3. _HandleBuildTransactionResponse()
 * 4. _Keccak256() [Current Step]
 * 5. _HandleKaccek256Response()
 * 6. _SignAndSubmitTransaction()
 * 7. _HandleSignAndSubmitTransaction()
 *
 * @param Halliday Pointer to the object that called Pointer to the object that called TransferAsset(), TransferBalance(), or ContractCall().
 * @param BuildTransactionResponse Response object from _BuildTransaction().
 * @param FromInGamePlayerId Player to build a transaction for.
 * @param TxType Type of transaction that is being called.
 */
void _Keccak256(AHalliday* Halliday, const FBuildTransactionResponse& BuildTransactionResponse, const FString& FromInGamePlayerId, ETransactionType TxType)
{
    FString TxHash = BuildTransactionResponse.tx_hash;
    FString GetPlayerWalletsUrl = Halliday->GetApiEndpoint() + TEXT("client/getKeccak256Hash?message=") + TxHash;
        
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(GetPlayerWalletsUrl);
    Request->SetVerb("GET");
    Request->SetHeader("Authorization", Halliday->GetAuthHeaderValue());
    
    // Bind a callback to handle the response.
    Request->OnProcessRequestComplete().BindLambda([Halliday, BuildTransactionResponse, FromInGamePlayerId, TxType](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
       _HandleKeccak256Response(Request, Response, bWasSuccessful, BuildTransactionResponse, Halliday, FromInGamePlayerId, TxType);
    });
    
    Request->ProcessRequest();
}

/**
 * Asynchronous callback function to handle the response of _BuildTransaction() and calls the Halliday backend to apply a Keccak256 hash to that transaction hash before signing it with the private key.
 * INTERNAL FLOW:
 * 1.  TransferAsset(), TransferBalance(), or ContractCall()
 * 2. _BuildTransaction()
 * 3. _HandleBuildTransactionResponse() [Current Step]
 * 4. _Keccak256()
 * 5. _HandleKaccek256Response()
 * 6. _SignAndSubmitTransaction()
 * 7. _HandleSignAndSubmitTransaction()
 *
 * @param Request Request sent from _BuildTransaction().
 * @param Response Response from the request sent from _BuildTransaction().
 * @param bWasSuccessful Indicates the success of the request.
 * @param Halliday Pointer to the object that called TransferAsset(), TransferBalance(), or ContractCall().
 * @param FromInGamePlayerId Id of the player to create a wallet for.
 * @param TxType  Type of transaction that is being called.
 */
void _HandleBuildTransactionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, AHalliday* Halliday, const FString& FromInGamePlayerId, ETransactionType TxType)
{
    if (bWasSuccessful && Response->GetResponseCode() == 200)
    {
        FString MessageBody = Response->GetContentAsString();
        FBuildTransactionResponse BuildTransactionResponse = ParseResponse<FBuildTransactionResponse>(MessageBody);
        
        // Use the server to hash the tx_hash
        _Keccak256(Halliday, BuildTransactionResponse, FromInGamePlayerId, TxType);
    }
    else
    {
        FString ResponseError = Response->GetContentAsString();
        UE_LOG(LogTemp, Error, TEXT("[Halliday Error] Failed to build a transaction of type '%s' for player '%s' because '%s'."), *(TransactionTypeToString(TxType)), *FromInGamePlayerId, *ResponseError);
    }
}

/**
 * Helper function that is used for TransferAsset(), TransferBalance(), and ContractCall(). Those high level functions only create the necessary body parameters.
 * INTERNAL FLOW:
 * 1.  TransferAsset(), TransferBalance(), or ContractCall()
 * 2. _BuildTransaction() [Current Step]
 * 3. _HandleBuildTransactionResponse()
 * 4. _Keccak256()
 * 5. _HandleKaccek256Response()
 * 6. _SignAndSubmitTransaction()
 * 7. _HandleSignAndSubmitTransaction()
 *
 * @param Halliday Pointer to the object that called GetOrCreateHallidayAAWallet().
 * @param TxType Type of transaction that is being called.
 * @param RequestBodyAsString Stringified request body to send to our backend.
 * @param FromInGamePlayerId Player to build a transaction for.
 */
void _BuildTransaction(AHalliday* Halliday, ETransactionType TxType, FString RequestBodyAsString, FString FromInGamePlayerId)
{
    FString BuildTransactionUrl = Halliday->GetApiEndpoint() + TEXT("client/transactions/") + TransactionTypeToString(TxType);
    
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BuildTransactionUrl);
    Request->SetVerb("POST");
    Request->SetHeader(TEXT("Authorization"), Halliday->GetAuthHeaderValue());
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(RequestBodyAsString);
    
    // Bind a callback to handle the response.
    Request->OnProcessRequestComplete().BindLambda([Halliday, FromInGamePlayerId, TxType](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
       _HandleBuildTransactionResponse(Request, Response, bWasSuccessful, Halliday, FromInGamePlayerId, TxType);
    });

    Request->ProcessRequest();
}

void AHalliday::TransferAsset(const FString& FromInGamePlayerId, const FString& ToInGamePlayerId, const FString& CollectionAddress, const FString& TokenId, EBlockchainType BlockchainType, bool bSponsorGas)
{
    TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
    RequestBody->SetStringField(TEXT("from_in_game_player_id"), FromInGamePlayerId);
    RequestBody->SetStringField(TEXT("to_in_game_player_id"), ToInGamePlayerId);
    RequestBody->SetStringField(TEXT("collection_address"), CollectionAddress);
    RequestBody->SetStringField(TEXT("token_id"), TokenId);
    RequestBody->SetStringField(TEXT("blockchain_type"), BlockchainTypeToString(BlockchainType));
    RequestBody->SetBoolField(TEXT("sponsor_gas"), bSponsorGas);

    FString RequestBodyAsString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyAsString);
    FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);
    
    _BuildTransaction(this, ETransactionType::TRANSFER_ASSET, RequestBodyAsString, FromInGamePlayerId);
}

void AHalliday::TransferBalance(const FString& FromInGamePlayerId, const FString& ToInGamePlayerId, EBlockchainType BlockchainType, bool bSponsorGas, const FString& Value, const FString& TokenAddress)
{
    TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
    RequestBody->SetStringField(TEXT("from_in_game_player_id"), FromInGamePlayerId);
    RequestBody->SetStringField(TEXT("to_in_game_player_id"), ToInGamePlayerId);
    RequestBody->SetStringField(TEXT("blockchain_type"), BlockchainTypeToString(BlockchainType));
    RequestBody->SetBoolField(TEXT("sponsor_gas"), bSponsorGas);
    RequestBody->SetStringField(TEXT("value"), Value);
    
    if(TokenAddress != "")
    {
        RequestBody->SetStringField(TEXT("token_address"), TokenAddress);
    }
    
    FString RequestBodyAsString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyAsString);
    FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);
    
    _BuildTransaction(this, ETransactionType::TRANSFER_BALANCE, RequestBodyAsString, FromInGamePlayerId);
}

void AHalliday::ContractCall(const FString& FromInGamePlayerId, const FString& TargetAddress, const FString& Calldata, EBlockchainType BlockchainType, bool bSponsorGas, const FString& Value)
{
    TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
    RequestBody->SetStringField(TEXT("from_in_game_player_id"), FromInGamePlayerId);
    RequestBody->SetStringField(TEXT("target_address"), TargetAddress);
    RequestBody->SetStringField(TEXT("value"), Value);
    RequestBody->SetStringField(TEXT("calldata"), Calldata);
    RequestBody->SetStringField(TEXT("blockchain_type"), BlockchainTypeToString(BlockchainType));
    RequestBody->SetBoolField(TEXT("sponsor_gas"), bSponsorGas);

    FString RequestBodyAsString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyAsString);
    FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);
    
    _BuildTransaction(this, ETransactionType::CALL_CONTRACT, RequestBodyAsString, FromInGamePlayerId);
}

// Called when the game starts or when spawned
void AHalliday::BeginPlay()
{
	Super::BeginPlay();
    
    // Create the two delegates to pass to Web3 Auth so we can execute our callback functions.
    FOnLogin LoginDelegate;
    FOnLogout LogoutDelegate;
    
    // Bind the callback functions that will initialize or clear our state.
    LoginDelegate.BindUFunction(this, FName("_HandleLogin"));
    LogoutDelegate.BindUFunction(this, FName("_HandleLogout"));

    // Call the Web3 Auth static functions to set the delegates on their end.
    AWeb3Auth::setLoginEvent(LoginDelegate);
    AWeb3Auth::setLogoutEvent(LogoutDelegate);
}

// Called every frame
void AHalliday::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
