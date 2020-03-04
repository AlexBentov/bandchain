type data_source_tab_t =
  | DataSourceExecute
  | DataSourceCode
  | DataSourceRequests
  | DataSourceRevisions;

type script_tab_t =
  | ScriptTransactions
  | ScriptCode
  | ScriptExecute
  | ScriptIntegration;

type request_tab_t =
  | RequestReportStatus
  | RequestProof;

type t =
  | NotFound
  | HomePage
  | DataSourceIndexPage(int, data_source_tab_t)
  | ScriptHomePage
  | ScriptIndexPage(Hash.t, script_tab_t)
  | TxHomePage
  | TxIndexPage(Hash.t)
  | BlockHomePage
  | BlockIndexPage(int)
  | RequestIndexPage(int, request_tab_t);

let fromUrl = (url: ReasonReactRouter.url) =>
  switch (url.path, url.hash) {
  | (["data-source", dataSourceID], "code") =>
    DataSourceIndexPage(dataSourceID |> int_of_string, DataSourceCode)
  | (["data-source", dataSourceID], "requests") =>
    DataSourceIndexPage(dataSourceID |> int_of_string, DataSourceRequests)
  | (["data-source", dataSourceID], "revisions") =>
    DataSourceIndexPage(dataSourceID |> int_of_string, DataSourceRevisions)
  | (["data-source", dataSourceID], _) =>
    DataSourceIndexPage(dataSourceID |> int_of_string, DataSourceExecute)
  | (["scripts"], _) => ScriptHomePage
  | (["script", codeHash], "code") => ScriptIndexPage(codeHash |> Hash.fromHex, ScriptCode)
  | (["script", codeHash], "execute") =>
    ScriptIndexPage(codeHash |> Hash.fromHex, ScriptExecute)
  | (["script", codeHash], "integration") =>
    ScriptIndexPage(codeHash |> Hash.fromHex, ScriptIntegration)
  | (["script", codeHash], _) => ScriptIndexPage(codeHash |> Hash.fromHex, ScriptTransactions)
  | (["txs"], _) => TxHomePage
  | (["tx", txHash], _) => TxIndexPage(Hash.fromHex(txHash))
  | (["blocks"], _) => BlockHomePage
  | (["block", blockHeight], _) =>
    let blockHeightIntOpt = blockHeight |> int_of_string_opt;
    BlockIndexPage(blockHeightIntOpt->Belt_Option.getWithDefault(0));
  | (["request", reqID], "proof") => RequestIndexPage(reqID |> int_of_string, RequestProof)
  | (["request", reqID], _) => RequestIndexPage(reqID |> int_of_string, RequestReportStatus)
  | ([], "") => HomePage
  | (_, _) => NotFound
  };

let toString =
  fun
  | DataSourceIndexPage(dataSourceID, DataSourceExecute) => {j|/data-source/$dataSourceID|j}
  | DataSourceIndexPage(dataSourceID, DataSourceCode) => {j|/data-source/$dataSourceID#code|j}
  | DataSourceIndexPage(dataSourceID, DataSourceRequests) => {j|/data-source/$dataSourceID#requests|j}
  | DataSourceIndexPage(dataSourceID, DataSourceRevisions) => {j|/data-source/$dataSourceID#revisions|j}
  | ScriptHomePage => "/scripts"
  | ScriptIndexPage(codeHash, ScriptTransactions) => {j|/script/$codeHash|j}
  | ScriptIndexPage(codeHash, ScriptCode) => {j|/script/$codeHash#code|j}
  | ScriptIndexPage(codeHash, ScriptExecute) => {j|/script/$codeHash#execute|j}
  | ScriptIndexPage(codeHash, ScriptIntegration) => {j|/script/$codeHash#integration|j}
  | TxHomePage => "/txs"
  | TxIndexPage(txHash) => {j|/tx/$txHash|j}
  | BlockHomePage => "/blocks"
  | BlockIndexPage(height) => {j|/block/$height|j}
  | RequestIndexPage(reqID, RequestReportStatus) => {j|/request/$reqID|j}
  | RequestIndexPage(reqID, RequestProof) => {j|/request/$reqID#proof|j}
  | HomePage
  | NotFound => "/";

let redirect = (route: t) => ReasonReactRouter.push(route |> toString);
