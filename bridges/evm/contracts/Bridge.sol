pragma solidity 0.5.14;
pragma experimental ABIEncoderV2;
import {BlockHeaderMerkleParts} from "./BlockHeaderMerkleParts.sol";
import {SafeMath} from "openzeppelin-solidity/contracts/math/SafeMath.sol";
import {Ownable} from "openzeppelin-solidity/contracts/ownership/Ownable.sol";
import {IAVLMerklePath} from "./IAVLMerklePath.sol";
import {TMSignature} from "./TMSignature.sol";
import {Utils} from "./Utils.sol";
import {IBridge} from "./IBridge.sol";


/// @title Bridge <3 BandChain D3N
/// @author Band Protocol Team
contract Bridge is IBridge, Ownable {
    using BlockHeaderMerkleParts for BlockHeaderMerkleParts.Data;
    using IAVLMerklePath for IAVLMerklePath.Data;
    using TMSignature for TMSignature.Data;
    using SafeMath for uint256;

    /// Mapping from block height to the hash of "zoracle" iAVL Merkle tree.
    mapping(uint256 => bytes32) public oracleStates;
    /// Mapping from an address to its voting power.
    mapping(address => uint256) public validatorPowers;
    /// The total voting power of active validators currently on duty.
    uint256 public totalValidatorPower;

    struct ValidatorWithPower {
        address addr;
        uint256 power;
    }

    /// Initializes an oracle bridge to BandChain.
    /// @param _validators The initial set of BandChain active validators.
    constructor(ValidatorWithPower[] memory _validators) public {
        for (uint256 idx = 0; idx < _validators.length; ++idx) {
            ValidatorWithPower memory validator = _validators[idx];
            require(
                validatorPowers[validator.addr] == 0,
                "DUPLICATION_IN_INITIAL_VALIDATOR_SET"
            );
            validatorPowers[validator.addr] = validator.power;
            totalValidatorPower = totalValidatorPower.add(validator.power);
        }
    }

    /// Update validator powers by owner.
    /// @param _validators The changed set of BandChain validators.
    function updateValidatorPowers(ValidatorWithPower[] memory _validators)
        public
        onlyOwner
    {
        for (uint256 idx = 0; idx < _validators.length; ++idx) {
            ValidatorWithPower memory validator = _validators[idx];
            totalValidatorPower = totalValidatorPower.sub(
                validatorPowers[validator.addr]
            );
            validatorPowers[validator.addr] = validator.power;
            totalValidatorPower = totalValidatorPower.add(validator.power);
        }
    }

    /// Relays a new oracle state to the bridge contract.
    /// @param _blockHeight The height of block to relay to this bridge contract.
    /// @param _oracleIAVLStateHash Hash of iAVL Merkle that represents the state of oracle store.
    /// @param _otherStoresMerkleHash Hash of internal Merkle node for other Tendermint storages.
    /// @param _merkleParts Extra merkle parts to compute block hash. See BlockHeaderMerkleParts lib.
    /// @param _signedDataPrefix Prefix data prepended prior to signing block hash.
    /// @param _signatures The signatures signed on this block, sorted alphabetically by address.
    function relayOracleState(
        uint256 _blockHeight,
        bytes32 _oracleIAVLStateHash,
        bytes32 _otherStoresMerkleHash,
        bytes32 _supplyStoresMerkleHash,
        BlockHeaderMerkleParts.Data memory _merkleParts,
        bytes memory _signedDataPrefix,
        TMSignature.Data[] memory _signatures
    ) public {
        // Computes Tendermint's application state hash at this given block. AppHash is actually a
        // Merkle hash on muliple stores. Luckily, we only care about "zoracle" tree and all other
        // stores can just be combined into one bytes32 hash off-chain.
        //
        //                                            ____________appHash_________
        //                                          /                              \
        //                   ____otherStoresMerkleHash ____                         ___innerHash___
        //                 /                                \                     /                  \
        //         _____ h5 ______                    ______ h6 _______        supply              zoracle
        //       /                \                 /                  \
        //     h1                  h2             h3                    h4
        //     /\                  /\             /\                    /\
        //  acc  distribution   gov  main     mint  params     slashing   staking
        bytes32 appHash = Utils.merkleInnerHash(
            _otherStoresMerkleHash,
            Utils.merkleInnerHash(
                _supplyStoresMerkleHash,
                Utils.merkleLeafHash(
                    abi.encodePacked(
                        hex"077a6f7261636c6520", // uint8(7) + "zoracle" + uint8(32)
                        sha256(
                            abi.encodePacked(
                                sha256(abi.encodePacked(_oracleIAVLStateHash))
                            )
                        )
                    )
                )
            )
        );
        // Computes Tendermint's block header hash at this given block.
        bytes32 blockHeader = _merkleParts.getBlockHeader(
            appHash,
            _blockHeight
        );
        // Counts the total number of valid signatures signed by active validators.
        address lastSigner = address(0);
        uint256 sumVotingPower = 0;
        for (uint256 idx = 0; idx < _signatures.length; ++idx) {
            address signer = _signatures[idx].recoverSigner(
                blockHeader,
                _signedDataPrefix
            );
            require(signer > lastSigner, "INVALID_SIGNATURE_SIGNER_ORDER");
            sumVotingPower = sumVotingPower.add(validatorPowers[signer]);
            lastSigner = signer;
        }
        // Verifies that sufficient validators signed the block and saves the oracle state.
        require(
            sumVotingPower.mul(3) > totalValidatorPower.mul(2),
            "INSUFFICIENT_VALIDATOR_SIGNATURES"
        );
        oracleStates[_blockHeight] = _oracleIAVLStateHash;
    }

    /// Helper struct to workaround Solidity's "stack too deep" problem.
    struct VerifyOracleDataLocalVariables {
        bytes encodedVarint;
        bytes32 dataHash;
    }

    // /// Decodes the encoded result and returns back the decoded data which is the data and its context.
    // /// @param _encodedData The encoded of result and its context.
    // function decodeResult(bytes memory _encodedData)
    //     public
    //     pure
    //     returns (VerifyOracleDataResult memory)
    // {
    //     require(_encodedData.length > 40, "INPUT_MUST_BE_LONGER_THAN_40_BYTES");

    //     VerifyOracleDataResult memory result;
    //     assembly {
    //         mstore(
    //             add(result, 0x20),
    //             and(mload(add(_encodedData, 0x08)), 0xffffffffffffffff)
    //         )
    //         mstore(
    //             add(result, 0x40),
    //             and(mload(add(_encodedData, 0x10)), 0xffffffffffffffff)
    //         )
    //         mstore(
    //             add(result, 0x60),
    //             and(mload(add(_encodedData, 0x18)), 0xffffffffffffffff)
    //         )
    //         mstore(
    //             add(result, 0x80),
    //             and(mload(add(_encodedData, 0x20)), 0xffffffffffffffff)
    //         )
    //         mstore(
    //             add(result, 0xa0),
    //             and(mload(add(_encodedData, 0x28)), 0xffffffffffffffff)
    //         )
    //     }

    //     bytes memory data = new bytes(_encodedData.length - 40);
    //     uint256 dataLengthInWords = ((data.length - 1) / 32) + 1;
    //     for (uint256 i = 0; i < dataLengthInWords; i++) {
    //         assembly {
    //             mstore(
    //                 add(data, add(0x20, mul(i, 0x20))),
    //                 mload(add(_encodedData, add(0x48, mul(i, 0x20))))
    //             )
    //         }
    //     }
    //     result.data = data;

    //     return result;
    // }

    /// Verifies that the given data is a valid data on BandChain as of the given block height.
    /// @param _blockHeight The block height. Someone must already relay this block.
    /// @param _data The data to verify, with the format similar to what on the blockchain store.
    /// @param _requestId The ID of request for this data piece.
    /// @param _version Lastest block height that the data node was updated.
    /// @param _merklePaths Merkle proof that shows how the data leave is part of the oracle iAVL.
    function verifyOracleData(
        uint256 _blockHeight,
        bytes memory _data,
        uint64 _requestId,
        uint64 _oracleScriptId,
        bytes memory _params,
        uint256 _version,
        IAVLMerklePath.Data[] memory _merklePaths
    ) public view returns (RequestPacket memory, ResponsePacket memory) {
        bytes32 oracleStateRoot = oracleStates[_blockHeight];
        require(
            oracleStateRoot != bytes32(uint256(0)),
            "NO_ORACLE_ROOT_STATE_DATA"
        );
        // Computes the hash of leaf node for iAVL oracle tree.
        VerifyOracleDataLocalVariables memory vars;
        vars.encodedVarint = Utils.encodeVarintSigned(_version);
        vars.dataHash = sha256(_data);
        bytes32 currentMerkleHash = sha256(
            abi.encodePacked(
                uint8(0), // Height of tree (only leaf node) is 0 (signed-varint encode)
                uint8(2), // Size of subtree is 1 (signed-varint encode)
                vars.encodedVarint,
                uint8(17 + _params.length), // Size of data key (1-byte constant 0x01 + 8-byte request ID + 8-byte oracleScriptId + length of params)
                uint8(255), // Constant 0xff prefix data request info storage key
                _requestId,
                _oracleScriptId,
                _params,
                uint8(32), // Size of data hash
                vars.dataHash
            )
        );
        // Goes step-by-step computing hash of parent nodes until reaching root node.
        for (uint256 idx = 0; idx < _merklePaths.length; ++idx) {
            currentMerkleHash = _merklePaths[idx].getParentHash(
                currentMerkleHash
            );
        }
        // Verifies that the computed Merkle root matches what currently exists.
        require(
            currentMerkleHash == oracleStateRoot,
            "INVALID_ORACLE_DATA_PROOF"
        );

        // VerifyOracleDataResult memory result = decodeResult(_data);
        // result.params = _params;
        // result.oracleScriptId = _oracleScriptId;

        // TODO: Return request and respond packet
        // return result;
        RequestPacket memory req;
        ResponsePacket memory res;

        return (req, res);
    }

    /// Performs oracle state relay and oracle data verification in one go. The caller submits
    /// the encoded proof and receives back the decoded data, ready to be validated and used.
    /// @param _data The encoded data for oracle state relay and data verification.
    function relayAndVerify(bytes calldata _data)
        external
        returns (RequestPacket memory, ResponsePacket memory)
    {
        (bytes memory relayData, bytes memory verifyData) = abi.decode(
            _data,
            (bytes, bytes)
        );
        (bool relayOk, ) = address(this).call(
            abi.encodePacked(this.relayOracleState.selector, relayData)
        );
        require(relayOk, "RELAY_ORACLE_STATE_FAILED");
        (bool verifyOk, bytes memory verifyResult) = address(this).staticcall(
            abi.encodePacked(this.verifyOracleData.selector, verifyData)
        );
        require(verifyOk, "VERIFY_ORACLE_DATA_FAILED");
        return abi.decode(verifyResult, (RequestPacket, ResponsePacket));
    }
}
