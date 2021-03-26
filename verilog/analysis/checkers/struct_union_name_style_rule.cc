// Copyright 2017-2020 The Verible Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "verilog/analysis/checkers/struct_union_name_style_rule.h"

#include <algorithm>
#include <set>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/strings/naming_utils.h"
#include "common/text/config_utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/type.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(StructUnionNameStyleRule);

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

absl::string_view StructUnionNameStyleRule::Name() {
  return "struct-union-name-style";
}
const char StructUnionNameStyleRule::kTopic[] = "struct-union-conventions";
const char StructUnionNameStyleRule::kMessageStruct[] =
    "Struct names must use lower_snake_case naming convention and end with _t.";
const char StructUnionNameStyleRule::kMessageUnion[] =
    "Union names must use lower_snake_case naming convention and end with _t.";

std::string StructUnionNameStyleRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that ", Codify("struct", description_type), " and ",
      Codify("union", description_type),
      " names use lower_snake_case naming convention and end with '_t'. See ",
      GetStyleGuideCitation(kTopic), ".");
}

static const Matcher &TypedefMatcher() {
  static const Matcher matcher(NodekTypeDeclaration());
  return matcher;
}

void StructUnionNameStyleRule::HandleSymbol(const verible::Symbol &symbol,
                                            const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (TypedefMatcher().Matches(symbol, &manager)) {
    const char *msg;
    // TODO: This can be changed to checking type of child (by index) when we
    // have consistent shape for all kTypeDeclaration nodes.
    if (!FindAllStructTypes(symbol).empty()) {
      msg = kMessageStruct;
    } else if (!FindAllUnionTypes(symbol).empty()) {
      msg = kMessageUnion;
    } else {
      // Neither a struct nor union definition
      return;
    }
    const auto *identifier_leaf = GetIdentifierFromTypeDeclaration(symbol);
    const auto name = ABSL_DIE_IF_NULL(identifier_leaf)->get().text();
    // get rid of the exceptions specified by user
    std::string name_filtered;
    if (!exceptions_.empty()) {
      name_filtered.reserve(name.length());
      const auto &underscore_separated_parts = absl::StrSplit(name, '_');
      if (name[0] != '_') {
        for (const auto &ns : underscore_separated_parts) {
          if (std::find(exceptions_.begin(), exceptions_.end(), ns) ==
              exceptions_.end()) {
            name_filtered.append(ns);
          }
        }
      } else {
        name_filtered.append("_");
      }
    }
    const absl::string_view relevant_name =
        exceptions_.empty() ? name : name_filtered;
    if (!verible::IsLowerSnakeCaseWithDigits(relevant_name) ||
        !absl::EndsWith(name, "_t")) {
      violations_.insert(LintViolation(identifier_leaf->get(), msg, context));
    }
  }
}

absl::Status StructUnionNameStyleRule::Configure(
    absl::string_view configuration) {
  using verible::config::SetString;
  std::string raw_tokens;
  auto status = verible::ParseNameValues(
      configuration, {{"exceptions", SetString(&raw_tokens)}});

  if (!raw_tokens.empty()) {
    const auto &exceptions = absl::StrSplit(raw_tokens, ',');
    for (const auto &ex : exceptions) {
      const auto &e =
          std::find_if_not(ex.begin(), ex.end(), absl::ascii_isalnum);
      if (e != ex.end())
        return absl::Status(absl::StatusCode::kInvalidArgument,
                            "The exception can be composed of digits and "
                            "alphabetic characters only");
      const auto &alpha =
          std::find_if(ex.begin(), ex.end(), absl::ascii_isalpha);
      if (alpha == ex.end())
        return absl::Status(
            absl::StatusCode::kInvalidArgument,
            "The exception have to contain at least one alphabetic character");

      const auto &digit = std::find_if(alpha, ex.end(), absl::ascii_isdigit);
      if (digit != ex.end())
        return absl::Status(absl::StatusCode::kInvalidArgument,
                            "Digits are forbidden when specifying the unit");

      exceptions_.emplace(ex);
    }
  }
  return status;
}

LintRuleStatus StructUnionNameStyleRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
