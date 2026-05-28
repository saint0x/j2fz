export interface Branded<TBrand extends string> {
  readonly __brand: TBrand;
}
