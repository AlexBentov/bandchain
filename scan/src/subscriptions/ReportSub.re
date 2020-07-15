module ValidatorReport = {
  type oracle_script_t = {
    id: ID.OracleScript.t,
    name: string,
  };

  type request_t = {
    id: ID.Request.t,
    oracleScript: oracle_script_t,
  };

  type raw_request_t = {dataSourceID: ID.DataSource.t};

  type report_details_t = {
    externalID: int,
    data: JsBuffer.t,
    rawRequest: option(raw_request_t),
  };

  type transaction_t = {hash: Hash.t};

  type internal_t = {
    request: request_t,
    transaction: transaction_t,
    reportDetails: array(report_details_t),
  };

  type t = {
    txHash: Hash.t,
    request: request_t,
    reportDetails: array(report_details_t),
  };

  let toExternal = ({request, transaction, reportDetails}) => {
    txHash: transaction.hash,
    request,
    reportDetails,
  };

  module MultiConfig = [%graphql
    {|
      subscription Reports ($limit: Int!, $offset: Int!, $validator: String!) {
        reports (where: {validator: {_eq: $validator}}, limit: $limit, offset: $offset, order_by: {request_id: desc}) @bsRecord {
            request @bsRecord {
              id @bsDecoder (fn: "ID.Request.fromInt")
              oracleScript: oracle_script @bsRecord {
                id @bsDecoder (fn: "ID.OracleScript.fromInt")
                name
              }
            }
            transaction @bsRecord{
              hash @bsDecoder (fn: "GraphQLParser.hash")
            }
            reportDetails: raw_reports @bsRecord {
              externalID: external_id @bsDecoder (fn:"GraphQLParser.int64")
              data @bsDecoder (fn: "GraphQLParser.buffer")
              rawRequest: raw_request @bsRecord {
                dataSourceID: data_source_id @bsDecoder (fn: "ID.DataSource.fromInt")
              }
            }
          }
        }
      |}
  ];

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
    let offset = (page - 1) * pageSize;
    let (result, _) =
      ApolloHooks.useSubscription(
        MultiConfig.definition,
        ~variables=MultiConfig.makeVariables(~limit=pageSize, ~offset, ~validator, ()),
      );
    result |> Sub.map(_, x => x##reports->Belt_Array.map(toExternal));
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
