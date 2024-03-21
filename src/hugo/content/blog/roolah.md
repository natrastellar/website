+++
title = "Introducing: Roolah"
subtitle = "Roolah Devlog #1"
date = 2022-11-27
summary = "Diagnosing and fixing a problem in my latest pet project."
tags = ["roolah", "rust", "development"]
toc = true
+++

I've been planning/working on a project for a little bit that I call Roolah. The names comes from the term "moolah", i.e. money, and the fact it's written in Rust. I thought it might be fun to share development updates!

## Motivation

Roolah is intended to be a replacement desktop app for the four year old Excel spreadsheet I use to track all of my finances - that I could say is anxiety-inspired because my Microsoft Office license finally expired - but, as is typical when it comes to my personal projects, that's not good enough. In the words of the late Kamina: "The magma of our soul burns with a mighty flame!" My eventual, lofty goal is to tack on some productivity/organization features that are tailored to how I work. That way I can take notes, track todo items, and update my budget in one place.

It's also an excuse to re-learn Rust and actually put it to use on something more substantial than daily Advent of Code exercises. In general, the project serves as a way of getting back into the habit of dev as a hobby. I'd like to be able to say I'm capable of more than just the routine C++ work that I do as a day job.

## Current Status

So, what have we got?

We're using a handful of dependencies:

```toml
miette = { version = "5.3", features = ["fancy"] }
thiserror = "1.0"
time = "0.3"
sqlx = { version = "0.6", features = ["runtime-tokio-rustls", "time", "sqlite", "offline", "decimal"] }
tokio = { version = "1.21", features = ["full"] }
rust_decimal = "1.26"
rust_decimal_macros = "1.26"
```

```rust
// We're still doing higher-level testing in main, for now.
#[tokio::main]
async fn main() -> Result<()> {
    const RECREATE_DATABASE: bool = true;
    let mut conn = database::init(RECREATE_DATABASE)
        .await
        .wrap_err("failed to initialize the database")?;

    let checking_account = database::create_account(&mut conn, "My Checking", &USD, "Checking")
        .await
        .wrap_err("failed to create a checking account")?;
    let accounts = database::get_all_accounts(&mut conn)
        .await
        .wrap_err("failed to get accounts")?;
    assert_eq!(1, accounts.len());
    assert_eq!(Some(&checking_account), accounts.first());
    let savings_account = database::create_account(&mut conn, "My Savings", &USD, "Savings")
        .await
        .wrap_err("failed to create a savings account")?;

    let checking_account_as_savings =
        database::create_account(&mut conn, "My Checking", &USD, "Savings").await;
    assert!(checking_account_as_savings.is_err());

    let mut cad = USD.into_owned();
    cad.name = Cow::Borrowed("Canadian Dollar");
    let canadian_checking_account =
        database::create_account(&mut conn, "My Checking", &cad, "Checking").await;
    assert!(canadian_checking_account.is_err());

    let _transfer = database::create_transaction(
        &mut conn,
        &date!(2022 - 10 - 6),
        &None,
        "",
        &dec!(5.00),
        checking_account.id,
        savings_account.id,
        "",
        "deposit",
        "transfer",
        None,
    )
    .await
    .wrap_err("failed to create a transfer")?;

    database::close(conn) // Checkpoints in WAL mode
        .await
        .wrap_err("failed to close the database")?;

    Ok(())
}
```

Things are pretty bare bones at the moment. We can setup a SQLite database, create some accounts in various currencies, and add transactions programmatically.

...right?

```powershell
PS C:\Programming\projects\roolah> cargo run
    Finished dev [unoptimized + debuginfo] target(s) in 0.29s
     Running `target\debug\roolah.exe`
Error: 
  × failed to create a transfer
  ├─▶ failed to get transaction by id
  ╰─▶ no rows returned by a query that expected to return at least one row      
```

Oops. Let's fix that.

## The Problem

```rust
pub async fn create_transaction(
    conn: &mut SqliteConnection,
    // ..., fields
) -> Result<Transaction> {
    let mut transaction = conn.begin().await.into_diagnostic()?;

    let category = match category {
        "" => None,
        _ => Some(create_category(&mut transaction, category).await?),
    };
    let method = match method {
        "" => None,
        _ => Some(create_method(&mut transaction, method).await?),
    };

    let inserted = sqlx::query(&format!(
        "INSERT INTO {transactions} ({date}, {posted_date}, {category}, {amount}, {debit_account}, {credit_account}, {authority}, {description}, {method}, {check_number})
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        RETURNING {id}
        ",
        // ..., identifiers
    ))
    // ... bindings
    .fetch_one(&mut transaction)
    .await
    .into_diagnostic()
    .wrap_err("failed to create transaction")?;

    transaction
        .commit()
        .await
        .into_diagnostic()
        .wrap_err("failed to commit")?;

    get_transaction_by_id(conn, inserted.try_get("id").into_diagnostic()?).await
}
```

It looks like the transaction is being created, but the logic to get it back out of the database is faulty.
Let's look at get_transaction_by_id.

```rust
pub async fn get_transaction_by_id(conn: &mut SqliteConnection, id: i64) -> Result<Transaction> {
    create_transactions_view(&mut *conn).await?;

    sqlx::query_as(&format!(
        "SELECT * FROM {transactions_view} WHERE {id} = ?",
        transactions_view = table_identifiers::TRANSACTIONS_WITH_CATEGORY_AND_METHOD,
        id = TransactionsWithCategoryAndMethodColumn::Id,
    ))
    .bind(id)
    .fetch_one(conn)
    .await
    .into_diagnostic()
    .wrap_err(format!("failed to get transaction with id {}", id))
}
```

Ooookay. Doesn't look like there are any problems here.
Maybe there's something wrong with the view?

```rust
// How do joins work again?
pub async fn create_transactions_view(conn: &mut SqliteConnection) -> Result<()> {
    sqlx::query(&format!(
        "CREATE VIEW IF NOT EXISTS {view} AS
        SELECT
            ... // columns
        FROM {transactions}
        INNER JOIN {categories}
            ON {transactions}.{category} = {categories}.{category_id}
        INNER JOIN {methods}
            ON {transactions}.{method} = {methods}.{method_id}",
        // ..., identifiers
    ))
    .execute(conn)
    .await
    .map(|_| ())
    .into_diagnostic()
    .wrap_err("failed to create transactions view")
}
```

Oh. Our transaction doesn't have a category, so the INNER JOINs are excluding the row we just added from the view.
Changing them to LEFT JOINs should do the trick.

## Computers Are Hard

Four hours later, it didn't actually do the trick.
I messed with the schema for a while, tried re-writing the queries, tried dropping the view explicitly, and did a lot of print-style debugging. I ended up deleting the database file entirely and it instantly started working.

ANYWAY. Lesson learned. Delete your database, or something. If that doesn't inspire confidence in my ability to write budgeting software, nothing will.