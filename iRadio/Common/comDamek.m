//
//  comDamek.m
//  iRadioHD
//
//  Created by Jan Damek on 14.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import "comDamek.h"

@implementation comDamek

+(NSString*)stringWithRemove:(NSString*) sender{
    NSString *str = [sender stringByReplacingOccurrencesOfString:@"\n" withString:@""];
    return [str stringByReplacingOccurrencesOfString:@"\t" withString:@""];
}

+(int)getInt:(NSDictionary*) sender{
    NSString *str = [comDamek stringWithRemove:[sender objectForKey:@"text"]];
    return  [str intValue];
}

+(NSNumber*)getNumber:(NSDictionary*) sender{
    int i = [comDamek getInt:sender];
    return [NSNumber numberWithInt:i];
}

+(NSString*)getString:(NSDictionary*) sender{
    return [comDamek stringWithRemove:[sender objectForKey:@"text"]];
}

@end
