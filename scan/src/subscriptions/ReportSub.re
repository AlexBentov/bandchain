module ValidatorReport = {
  type oracle_script_t = {
    id: ID.OracleScript.t,
    name: string,
  };

  type request_t = {
    id: ID.Request.t,
    oracleScript: oracle_script_t,
  };

  type report_details_t = {
    dataSourceID: ID.DataSource.t,
    externalID: int,
    data: JsBuffer.t,
  };

  type t = {
    txHash: Hash.t,
    request: request_t,
    reportDetails: array(report_details_t),
  };

  // module MultiConfig = [%graphql
  //   {|
  //     subscription Reports ($limit: Int!, $offset: Int!, $validator: String!) {
  //       reports (where: {validator: {_eq: $validator}}, limit: $limit, offset: $offset, order_by: {request_id: desc}) @bsRecord {
  //           request @bsRecord {
  //             id @bsDecoder (fn: "ID.Request.fromInt")
  //             oracleScript: oracle_script @bsRecord {
  //               id @bsDecoder (fn: "ID.OracleScript.fromInt")
  //               name
  //             }
  //           }
  //           txHash: tx_hash @bsDecoder (fn: "GraphQLParser.hash")
  //           reportDetails: report_details @bsRecord {
  //             dataSourceID: data_source_id @bsDecoder (fn: "ID.DataSource.fromInt")
  //             externalID: external_id @bsDecoder (fn: "GraphQLParser.int64")
  //             data @bsDecoder (fn: "GraphQLParser.buffer")
  //           }
  //         }
  //       }
  //     |}
  // ];

  module ReportCountConfig = [%graphql
    {|
    subscription ReportsCount ($validator: String!) {
      reports_aggregate(where: {validator: {_eq: $validator}}) {
        aggregate{
          count @bsDecoder(fn: "Belt_Option.getExn")
        }
      }
    }
  |}
  ];

  let getListByValidator = (~page=1, ~pageSize=5, ~validator) => {
    // let offset = (page - 1) * pageSize;
    // let (result, _) =
    //   ApolloHooks.useSubscription(
    //     MultiConfig.definition,
    //     ~variables=MultiConfig.makeVariables(~limit=pageSize, ~offset, ~validator, ()),
    //   );
    // result |> Sub.map(_, x => x##reports);
    Sub.resolve([|
      {
        txHash: "89e6fd29d5e8c37af39351695b91d1b58b85071485794024ca7bb81d7becd1ac" |> Hash.fromHex,
        request: {
          id: ID.Request.ID(1),
          oracleScript: {
            id: ID.OracleScript.ID(3),
            name: "jjjj",
          },
        },
        reportDetails: [||],
      },
    |]);
  };

  let count = validator => {
    let (result, _) =
      ApolloHooks.useSubscription(
        ReportCountConfig.definition,
        ~variables=ReportCountConfig.makeVariables(~validator, ()),
      );
    result
    |> Sub.map(_, x => x##reports_aggregate##aggregate |> Belt_Option.getExn |> (y => y##count));
  };
};
