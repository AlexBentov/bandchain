type amount_t = {
  amount: string,
  denom: string,
};

type fee_t = {
  amount: array(amount_t),
  gas: string,
};

type msg_send_t = {
  to_address: string,
  from_address: string,
  amount: array(amount_t),
};

type msg_delegate_t = {
  delegator_address: string,
  validator_address: string,
  amount: amount_t,
};

type msg_request_t = {
  oracleScriptID: string,
  calldata: string,
  requestedValidatorCount: string,
  sufficientValidatorCount: string,
  sender: string,
  clientID: string,
};

type msg_input_t =
  | Send(Address.t, amount_t)
  | Delegate(Address.t, amount_t)
  | Undelegate(Address.t, amount_t)
  | Request(ID.OracleScript.t, JsBuffer.t, string, string, Address.t, string);

type msg_payload_t = {
  [@bs.as "type"]
  type_: string,
  value: Js.Json.t,
};

type account_result_t = {
  accountNumber: int,
  sequence: int,
};

type pub_key_t = {
  [@bs.as "type"]
  type_: string,
  value: string,
};

type signature_t = {
  pub_key: Js.Json.t,
  public_key: string,
  signature: string,
};

type raw_tx_t = {
  msgs: array(msg_payload_t),
  chain_id: string,
  fee: fee_t,
  memo: string,
  account_number: string,
  sequence: string,
};

type signed_tx_t = {
  fee: fee_t,
  memo: string,
  msg: array(msg_payload_t),
  signatures: array(signature_t),
};

type t = {
  mode: string,
  tx: signed_tx_t,
};

type tx_response_t = {
  txHash: Hash.t,
  rawLog: string,
  success: bool,
};

type response_t =
  | Tx(tx_response_t)
  | Unknown;

let getAccountInfo = address => {
  let url = Env.rpc ++ "/auth/accounts/" ++ (address |> Address.toBech32);
  let%Promise info = Axios.get(url);
  let data = info##data;
  Promise.ret(
    JsonUtils.Decode.{
      accountNumber: data |> at(["result", "value", "account_number"], intstr),
      sequence:
        data
        |> optional(at(["result", "value", "sequence"], intstr))
        |> Belt_Option.getWithDefault(_, 0),
    },
  );
};

let stringifyWithSpaces: raw_tx_t => string = [%bs.raw
  {|
  function stringifyWithSpaces(obj) {
    return JSON.stringify(obj, undefined, 4);
  }
|}
];

let sortAndStringify: raw_tx_t => string = [%bs.raw
  {|
  function sortAndStringify(obj) {
    function sortObject(obj) {
      if (obj === null) return null;
      if (typeof obj !== "object") return obj;
      if (Array.isArray(obj)) return obj.map(sortObject);
      const sortedKeys = Object.keys(obj).sort();
      const result = {};
      sortedKeys.forEach(key => {
        result[key] = sortObject(obj[key])
      });
      return result;
    }

    return JSON.stringify(sortObject(obj));
  }
|}
];

let createMsg = (sender, msg: msg_input_t): msg_payload_t => {
  let msgType =
    switch (msg) {
    | Send(_) => "cosmos-sdk/MsgSend"
    | Delegate(_) => "cosmos-sdk/MsgDelegate"
    | Undelegate(_) => "cosmos-sdk/MsgUndelegate"
    | Request(_) => "oracle/Request"
    };

  let msgValue =
    switch (msg) {
    | Send(toAddress, coins) =>
      Js.Json.stringifyAny({
        from_address: sender |> Address.toBech32,
        to_address: toAddress |> Address.toBech32,
        amount: [|coins|],
      })
      |> Belt_Option.getExn
      |> Js.Json.parseExn
    | Delegate(validator, amount) =>
      {
        Js.Json.stringifyAny({
          delegator_address: sender |> Address.toBech32,
          validator_address: validator |> Address.toOperatorBech32,
          amount,
        });
      }
      |> Belt_Option.getExn
      |> Js.Json.parseExn
    | Undelegate(validator, amount) =>
      {
        Js.Json.stringifyAny({
          delegator_address: sender |> Address.toBech32,
          validator_address: validator |> Address.toOperatorBech32,
          amount,
        });
      }
      |> Belt_Option.getExn
      |> Js.Json.parseExn
    | Request(
        ID.OracleScript.ID(oracleScriptID),
        calldata,
        requestedValidatorCount,
        sufficientValidatorCount,
        sender,
        clientID,
      ) =>
      Js.Json.stringifyAny({
        oracleScriptID: oracleScriptID |> string_of_int,
        calldata: calldata |> JsBuffer.toBase64,
        requestedValidatorCount,
        sufficientValidatorCount,
        sender: sender |> Address.toBech32,
        clientID,
      })
      |> Belt_Option.getExn
      |> Js.Json.parseExn
    };
  {type_: msgType, value: msgValue};
};

let createRawTx = (~address, ~msgs, ~chainID, ~feeAmount, ~gas, ~memo, ()) => {
  let%Promise accountInfo = getAccountInfo(address);
  Promise.ret({
    msgs: msgs->Belt_Array.map(createMsg(address)),
    chain_id: chainID,
    fee: {
      amount: [|{amount: feeAmount, denom: "uband"}|],
      gas,
    },
    memo,
    account_number: accountInfo.accountNumber |> string_of_int,
    sequence: accountInfo.sequence |> string_of_int,
  });
};

let createSignedTx = (~network, ~signature, ~pubKey, ~tx: raw_tx_t, ~mode, ()) => {
  let newPubKey = "eb5ae98721" ++ (pubKey |> PubKey.toHex) |> JsBuffer.hexToBase64;
  let signedTx = {
    fee: tx.fee,
    memo: tx.memo,
    msg: tx.msgs,
    signatures: [|
      {
        pub_key: {
          exception WrongNetwork(string);
          switch (network) {
          | "GUANYU" => Js.Json.string(newPubKey)
          | "WENCHANG" =>
            Js.Json.object_(
              Js.Dict.fromList([
                ("type", Js.Json.string("tendermint/PubKeySecp256k1")),
                ("value", Js.Json.string(pubKey |> PubKey.toBase64)),
              ]),
            )
          | _ => raise(WrongNetwork("Incorrect or unspecified NETWORK environment variable"))
          };
        },
        public_key: newPubKey,
        signature,
      },
    |],
  };
  {mode, tx: signedTx};
};

let broadcast = signedTx => {
  /* TODO: FIX THIS MESS */
  let convert: t => Js.t('a) = [%bs.raw {|
function(data) {return {...data};}
  |}];

  let%Promise rawResponse = Axios.postData(Env.rpc ++ "/txs", convert(signedTx));
  let response = rawResponse##data;
  Promise.ret(
    Tx(
      JsonUtils.Decode.{
        txHash: response |> at(["txhash"], string) |> Hash.fromHex,
        rawLog: response |> at(["raw_log"], string),
        success: response |> optional(field("logs", _ => ())) |> Belt_Option.isSome,
      },
    ),
  );
};
