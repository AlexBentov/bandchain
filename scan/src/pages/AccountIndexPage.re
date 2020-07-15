module Styles = {
  open Css;

  let vFlex = style([display(`flex), flexDirection(`row), alignItems(`center)]);

  let addressContainer = style([display(`flex), flexDirection(`row), alignItems(`center)]);

  let logo = style([width(`px(50)), marginRight(`px(10))]);

  let cFlex = style([display(`flex), flexDirection(`column), alignItems(`flexEnd)]);

  let rFlex = style([display(`flex), flexDirection(`row)]);

  let separatorLine =
    style([
      width(`px(1)),
      height(`px(275)),
      backgroundColor(Colors.gray7),
      marginLeft(`px(20)),
      opacity(0.3),
    ]);

  let ovalIcon = color =>
    style([
      width(`px(17)),
      height(`px(17)),
      backgroundColor(color),
      borderRadius(`percent(50.)),
    ]);

  let balance = style([minWidth(`px(150)), justifyContent(`flexEnd)]);

  let totalContainer =
    style([
      display(`flex),
      flexDirection(`column),
      justifyContent(`spaceBetween),
      alignItems(`flexEnd),
      height(`px(200)),
      padding2(~v=`px(12), ~h=`zero),
    ]);

  let totalBalance = style([display(`flex), flexDirection(`column), alignItems(`flexEnd)]);

  let button =
    style([
      backgroundColor(Colors.blue1),
      padding2(~h=`px(8), ~v=`px(4)),
      display(`flex),
      borderRadius(`px(6)),
      cursor(`pointer),
      boxShadow(Shadow.box(~x=`zero, ~y=`px(2), ~blur=`px(4), rgba(20, 32, 184, 0.2))),
      borderRadius(`px(10)),
    ]);
};

let balanceDetail = (~title, ~description, ~amount, ~usdPrice, ~color, ~isCountup=false, ()) => {
  <Row alignItems=Css.flexStart>
    <Col size=0.25> <div className={Styles.ovalIcon(color)} /> </Col>
    <Col size=1.2>
      <Text
        value=title
        size=Text.Sm
        height={Text.Px(18)}
        spacing={Text.Em(0.03)}
        nowrap=true
        tooltipItem={description |> React.string}
        tooltipPlacement=Text.AlignBottomStart
      />
    </Col>
    <Col size=0.6>
      <div className=Styles.cFlex>
        <div className=Styles.rFlex>
          {isCountup
             ? <NumberCountup
                 value=amount
                 size=Text.Lg
                 weight=Text.Semibold
                 spacing={Text.Em(0.02)}
               />
             : <Text
                 value={amount |> Format.fPretty}
                 size=Text.Lg
                 weight=Text.Semibold
                 spacing={Text.Em(0.02)}
                 nowrap=true
                 code=true
               />}
          <HSpacing size=Spacing.sm />
          <Text
            value="BAND"
            size=Text.Lg
            code=true
            weight=Text.Thin
            spacing={Text.Em(0.02)}
            nowrap=true
          />
        </div>
        <VSpacing size=Spacing.xs />
        <div className={Css.merge([Styles.rFlex, Styles.balance])}>
          {isCountup
             ? <NumberCountup
                 value={amount *. usdPrice}
                 size=Text.Sm
                 weight=Text.Thin
                 spacing={Text.Em(0.02)}
               />
             : <Text
                 value={amount *. usdPrice |> Format.fPretty}
                 size=Text.Sm
                 spacing={Text.Em(0.02)}
                 weight=Text.Thin
                 nowrap=true
                 code=true
               />}
          <HSpacing size=Spacing.sm />
          <Text
            value="USD"
            size=Text.Sm
            code=true
            spacing={Text.Em(0.02)}
            weight=Text.Thin
            nowrap=true
          />
        </div>
      </div>
    </Col>
  </Row>;
};

let totalBalanceRender = (title, amount, symbol) => {
  <div className=Styles.totalBalance>
    <Text value=title size=Text.Md spacing={Text.Em(0.03)} height={Text.Px(18)} />
    <VSpacing size=Spacing.md />
    <div className=Styles.rFlex>
      <NumberCountup value=amount size=Text.Xxl weight=Text.Semibold spacing={Text.Em(0.02)} />
      <HSpacing size=Spacing.sm />
      <Text value=symbol size=Text.Xxl weight=Text.Thin spacing={Text.Em(0.02)} code=true />
    </div>
  </div>;
};

[@react.component]
let make = (~address, ~hashtag: Route.account_tab_t) =>
  {
    let accountSub = AccountSub.get(address);
    let trackingSub = TrackingSub.use();
    let balanceAtStakeSub = DelegationSub.getTotalStakeByDelegator(address);
    let unbondingSub = UnbondingSub.getUnbondingBalance(address);
    let infoSub = React.useContext(GlobalContext.context);
    let (_, dispatchModal) = React.useContext(ModalContext.context);
    let (accountOpt, _) = React.useContext(AccountContext.context);

    let%Sub info = infoSub;
    let%Sub account = accountSub;
    let%Sub balanceAtStake = balanceAtStakeSub;
    let%Sub unbonding = unbondingSub;
    let%Sub tracking = trackingSub;

    let usdPrice = info.financial.usdPrice;

    let availableBalance = account.balance->Coin.getBandAmountFromCoins;
    let balanceAtStakeAmount = balanceAtStake.amount->Coin.getBandAmountFromCoin;
    let rewardAmount = balanceAtStake.reward->Coin.getBandAmountFromCoin;
    let unbondingAmount = unbonding->Coin.getBandAmountFromCoin;

    let totalBalance = availableBalance +. balanceAtStakeAmount +. rewardAmount +. unbondingAmount;
    let send = () => {
      switch (accountOpt) {
      | Some({address: sender}) =>
        let openSendModal = () =>
          dispatchModal(OpenModal(SubmitTx(SubmitMsg.Send(Some(address)))));
        if (sender == address) {
          Window.confirm("Are you sure you want to send tokens to yourself?")
            ? openSendModal() : ();
        } else {
          openSendModal();
        };
      | None => dispatchModal(OpenModal(Connect(tracking.chainID)))
      };
    };

    <>
      <Row justify=Row.Between>
        <Col>
          <div className=Styles.vFlex>
            <img src=Images.accountLogo className=Styles.logo />
            <Text
              value="ACCOUNT DETAIL"
              weight=Text.Medium
              size=Text.Md
              spacing={Text.Em(0.06)}
              height={Text.Px(15)}
              nowrap=true
              color=Colors.gray7
              block=true
            />
          </div>
        </Col>
      </Row>
      <VSpacing size=Spacing.lg />
      <VSpacing size=Spacing.sm />
      <div className=Styles.addressContainer>
        <AddressRender address position=AddressRender.Title copy=true clickable=false />
        <HSpacing size=Spacing.md />
        <div className=Styles.button onClick={_ => {send()}}>
          <Text
            value="Send BAND"
            size=Text.Lg
            block=true
            color=Colors.blue7
            nowrap=true
            weight=Text.Medium
          />
        </div>
      </div>
      <VSpacing size=Spacing.xxl />
      <Row justify=Row.Between alignItems=`flexStart>
        <Col size=0.75>
          <PieChart
            size=187
            availableBalance
            balanceAtStake=balanceAtStakeAmount
            reward=rewardAmount
            unbonding=unbondingAmount
          />
        </Col>
        <Col size=1.>
          <VSpacing size=Spacing.md />
          {balanceDetail(
             ~title="AVAILABLE BALANCE",
             ~description="Balance available to send, delegate, etc",
             ~amount=availableBalance,
             ~usdPrice,
             ~color=Colors.bandBlue,
             (),
           )}
          <VSpacing size=Spacing.lg />
          <VSpacing size=Spacing.md />
          {balanceDetail(
             ~title="BALANCE AT STAKE",
             ~description="Balance currently delegated to validators",
             ~amount=balanceAtStakeAmount,
             ~usdPrice,
             ~color=Colors.chartBalanceAtStake,
             (),
           )}
          <VSpacing size=Spacing.lg />
          <VSpacing size=Spacing.md />
          {balanceDetail(
             ~title="UNBONDING AMOUNT",
             ~description="Amount undelegated from validators awaiting 21 days lockup period",
             ~amount=unbondingAmount,
             ~usdPrice,
             ~color=Colors.blue4,
             (),
           )}
          <VSpacing size=Spacing.lg />
          <VSpacing size=Spacing.md />
          {balanceDetail(
             ~title="REWARD",
             ~description="Reward from staking to validators",
             ~amount=rewardAmount,
             ~usdPrice,
             ~color=Colors.chartReward,
             ~isCountup=true,
             (),
           )}
        </Col>
        <div className=Styles.separatorLine />
        <Col size=1. alignSelf=Col.Start>
          <div className=Styles.totalContainer>
            {totalBalanceRender("TOTAL BAND BALANCE", totalBalance, "BAND")}
            {totalBalanceRender(
               "TOTAL BAND IN USD ($" ++ (usdPrice |> Format.fPretty) ++ " / BAND)",
               totalBalance *. usdPrice,
               "USD",
             )}
          </div>
        </Col>
      </Row>
      <VSpacing size=Spacing.xl />
      <Tab
        tabs=[|
          {
            name: "TRANSACTIONS",
            route: Route.AccountIndexPage(address, Route.AccountTransactions),
          },
          {
            name: "DELEGATIONS",
            route: Route.AccountIndexPage(address, Route.AccountDelegations),
          },
        |]
        currentRoute={Route.AccountIndexPage(address, hashtag)}>
        {switch (hashtag) {
         | AccountTransactions => <AccountIndexTransactions accountAddress=address />
         | AccountDelegations => <AccountIndexDelegations address />
         }}
      </Tab>
    </>
    |> Sub.resolve;
  }
  |> Sub.default(_, React.null);
